#include <WiFi.h>
#include <WebServer.h>
#include "server.h"
#include "servo_control.h"   // drop(), lift(), windUp(), reset(), servo_sleep(), servo_wake_up(), enterAutoMode(), enterManualMode(), windSeconds
#include "MQTT_config.h"     // autoMode, detectionThreshold, bmanUp, servoAwake

static WebServer server(80);
static bool* pAutoMode = nullptr;
static const char* last_action = "none";

static const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Boogieman Control</title>
<style>
:root{font-family:system-ui,Segoe UI,Roboto,Arial,sans-serif}
body{margin:20px;max-width:900px}
.row{display:flex;gap:12px;align-items:center;margin:12px 0;flex-wrap:wrap}
.card{border:1px solid #ddd;border-radius:12px;padding:12px}
button{font-size:16px;padding:10px 16px;cursor:pointer}
input[type=number]{font-size:16px;padding:8px;width:140px}
.pill{border:1px solid #ccc;border-radius:999px;padding:6px 12px}
#status{margin-top:8px;font:14px monospace;white-space:pre-wrap}
.title{font-weight:600;margin-bottom:6px}
input[type=checkbox]{width:22px;height:22px;vertical-align:middle}
</style>

<h2>Boogieman Control</h2>

<div class="card">
  <div class="row">
    <span class="pill">Mode:</span>
    <label><input id="modeToggle" type="checkbox"> Auto</label>
    <span>(unchecked = Manual)</span>
  </div>
  <div class="row">
    <button id="btnDrop">Drop</button>
    <button id="btnLift">Lift</button>
    <button id="btnWind">Wind</button>
  </div>
</div>

<div class="card">
  <div class="title">System</div>
  <div class="row">
    <button id="btnReset">Reset</button>
    <button id="btnSleep">Sleep</button>
    <button id="btnWake">Wake</button>
    <button id="btnReboot">Reboot</button>
  </div>
</div>

<div class="card">
  <div class="title">Parameters</div>
  <div class="row">
    <input id="thInput" type="number" min="5" max="400" step="1" placeholder="threshold cm">
    <button id="btnSetTh">Set threshold</button>
  </div>
  <div class="row">
    <input id="wtInput" type="number" min="0.1" max="60" step="0.1" placeholder="wind sec">
    <button id="btnSetWt">Set wind time</button>
  </div>
</div>

<div class="card">
  <div class="title">Status</div>
  <div id="status">Loading…</div>
</div>

<script>
async function req(u,opt){const r=await fetch(u,opt); if(!r.ok) throw new Error(await r.text()); return r;}

// track editing state
const editing = { th:false, wt:false };

async function getStatus(){
  const j=await (await fetch('/status')).json();
  modeToggle.checked = (j.mode==='auto');
  if (!editing.th) thInput.value = j.threshold;
  if (!editing.wt) wtInput.value = j.windSeconds.toFixed(2);
  status.textContent=`mode=${j.mode}  bmanUp=${j.bmanUp}  servoAwake=${j.servoAwake}
threshold=${j.threshold}cm  windSeconds=${j.windSeconds}
last_action=${j.last_action}
ip=${j.ip}`;
}

async function setMode(autoOn){ await req(`/set_mode?mode=${autoOn?'auto':'manual'}`,{method:'POST'}); await getStatus(); }
async function doAction(cmd){ await req(`/action?cmd=${cmd}`,{method:'POST'}); await getStatus(); }
async function sys(cmd){ await req(`/system?cmd=${cmd}`,{method:'POST'}); await getStatus(); }
async function setParam(k,v){ await req(`/set?${k}=${encodeURIComponent(v)}`,{method:'POST'}); await getStatus(); }

// event wiring
modeToggle.addEventListener('change',e=>setMode(e.target.checked));
btnDrop.addEventListener('click',_=>doAction('drop'));
btnLift.addEventListener('click',_=>doAction('lift'));
btnWind.addEventListener('click',_=>doAction('wind'));
btnReset.addEventListener('click',_=>sys('reset'));
btnSleep.addEventListener('click',_=>sys('sleep'));
btnWake.addEventListener('click',_=>sys('wake'));
btnReboot.addEventListener('click',_=>sys('reboot'));

thInput.addEventListener('focus', ()=>editing.th=true);
thInput.addEventListener('blur',  ()=>{editing.th=false; getStatus();});
btnSetTh.addEventListener('click', _=>{
  const v=thInput.value; if(!v) return;
  editing.th=false; setParam('threshold', v);
});

wtInput.addEventListener('focus', ()=>editing.wt=true);
wtInput.addEventListener('blur',  ()=>{editing.wt=false; getStatus();});
btnSetWt.addEventListener('click', _=>{
  const v=wtInput.value; if(!v) return;
  editing.wt=false; setParam('wind', v);
});

// poll, but skip if editing either field
getStatus();
setInterval(()=>{ if(!editing.th && !editing.wt) getStatus(); }, 1000);
</script>
)HTML";

static void handleRoot(){ server.send_P(200,"text/html",INDEX_HTML); }

static void handleStatus(){
  char buf[320];
  const char* modeStr=(pAutoMode&&*pAutoMode)?"auto":"manual";
  IPAddress ip=WiFi.localIP();
  snprintf(buf,sizeof(buf),
    "{\"mode\":\"%s\",\"bmanUp\":%s,\"servoAwake\":%s,"
    "\"threshold\":%d,\"windSeconds\":%.2f,"
    "\"last_action\":\"%s\",\"ip\":\"%u.%u.%u.%u\"}",
    modeStr, bmanUp?"true":"false", servoAwake?"true":"false",
    detectionThreshold, windSeconds,
    last_action, ip[0],ip[1],ip[2],ip[3]);
  server.send(200,"application/json",buf);
}

static void handleSetMode() {
  if (!pAutoMode) { server.send(500, "text/plain", "mode ptr null"); return; }
  if (!server.hasArg("mode")) { server.send(400, "text/plain", "missing mode"); return; }
  String m = server.arg("mode");
  if (m == "auto")   { *pAutoMode = true;  enterAutoMode();  }
  else if (m == "manual") { *pAutoMode = false; enterManualMode(); }
  else { server.send(400, "text/plain", "invalid mode"); return; }
  server.send(200, "text/plain", "ok");
}
static void handleAction(){
  if(!server.hasArg("cmd")){ server.send(400,"text/plain","missing cmd"); return; }
  String c=server.arg("cmd");
  if(c=="drop"){ drop(); last_action="drop"; }
  else if(c=="lift"){ lift(); last_action="lift"; }
  else if(c=="wind"){ windUp(windSeconds); last_action="wind"; }
  else { server.send(400,"text/plain","invalid cmd"); return; }
  server.send(200,"text/plain","ok");
}

static void handleSystem(){
  if(!server.hasArg("cmd")){ server.send(400,"text/plain","missing cmd"); return; }
  String c=server.arg("cmd");
  if(c=="reset"){ reset(); last_action="reset"; }
  else if(c=="sleep"){ servo_sleep(); last_action="sleep"; }
  else if(c=="wake"){ servo_wake_up(); last_action="wake"; }
  else if(c=="reboot"){ server.send(200,"text/plain","restarting"); delay(150); ESP.restart(); return; }
  else { server.send(400,"text/plain","invalid cmd"); return; }
  server.send(200,"text/plain","ok");
}

static void handleSet(){
  bool changed=false;
  if(server.hasArg("threshold")){
    int v=server.arg("threshold").toInt();
    if(v>=5 && v<=400){ detectionThreshold=v; changed=true; }
    else { server.send(400,"text/plain","bad threshold"); return; }
  }
  if(server.hasArg("wind")){
    float w=server.arg("wind").toFloat();
    if(w>0.0f && w<=60.0f){ windSeconds=w; changed=true; }
    else { server.send(400,"text/plain","bad windTime"); return; }
  }
  if(!changed){ server.send(400,"text/plain","no params"); return; }
  server.send(200,"text/plain","ok");
}

static void handleNotFound(){ server.send(404,"text/plain","not found"); }

void setupWebServer(bool* autoModePtr){
  pAutoMode=autoModePtr;
  server.on("/",        HTTP_GET,  handleRoot);
  server.on("/status",  HTTP_GET,  handleStatus);
  server.on("/set_mode",HTTP_POST, handleSetMode);
  server.on("/action",  HTTP_POST, handleAction);
  server.on("/system",  HTTP_POST, handleSystem);
  server.on("/set",     HTTP_POST, handleSet);
  server.onNotFound(handleNotFound);
  server.begin();
}

void webServerLoop(){ server.handleClient(); }
