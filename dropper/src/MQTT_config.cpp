#include "MQTT_config.h"
#include "servo_control.h"
#include <ESPmDNS.h>

WiFiClient espClient;
PubSubClient client(espClient);

// Track whether we were in auto mode before going to sleep so we can restore it on wake
bool wasAutoBeforeSleep = false;

// Helper to publish an acknowledgement. If reqId is empty, publish a simple message.
void publishAck(const String &reqId, const char *status, const char *text) {
    // Strict JSON reply with timestamp in milliseconds
    char buf[512];
    unsigned long ts = millis();
    if (reqId.length() > 0) {
        snprintf(buf, sizeof(buf), "{\"id\":\"%s\",\"status\":\"%s\",\"msg\":\"%s\",\"ts\":%lu}", reqId.c_str(), status, text, ts);
    } else {
        snprintf(buf, sizeof(buf), "{\"status\":\"%s\",\"msg\":\"%s\",\"ts\":%lu}", status, text, ts);
    }
    client.publish(status_reply_topic, buf, false);
}

// Publish larger command results (not acknowledgements) to a separate topic
void publishResult(const String &reqId, const char *text) {
    // If reqId present include it in JSON; otherwise just publish the text
    char buf[1024];
    unsigned long ts = millis();
    if (reqId.length() > 0) {
        snprintf(buf, sizeof(buf), "{\"id\":\"%s\",\"result\":\"%s\",\"ts\":%lu}", reqId.c_str(), text, ts);
    } else {
        snprintf(buf, sizeof(buf), "{\"result\":\"%s\",\"ts\":%lu}", text, ts);
    }
    client.publish(status_result_topic, buf, false);
}

// Publish a retained JSON summary of current status for dashboards
void publishStatusSummary() {
    char buf[256];
    unsigned long ts = millis();
    const char *mode = autoMode ? "auto" : "manual";
    const char *state = servoAwake ? "awake" : "sleeping";
    snprintf(buf, sizeof(buf), "{\"mode\":\"%s\",\"state\":\"%s\",\"threshold\":%d,\"windSeconds\":%.2f,\"bmanUp\":%s,\"ts\":%lu}", mode, state, detectionThreshold, windSeconds, bmanUp?"true":"false", ts);
    client.publish(status_topic_state, buf, true);
}


void setupWiFi() {
    WiFi.begin(wifi_ssid, wifi_password);
    Serial.println("Connecting to WiFi");
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        // If not connected within 10 seconds, restart the MCU to attempt recovery
        if (millis() - start >= 10000) {
            Serial.println();
            Serial.println("WiFi connection timeout (10s). Restarting...");
            delay(200);
            ESP.restart();
        }
    }
    Serial.println("");
    Serial.println("Connected to WiFi");
}

void setupMQTT() {
    Serial.println("Setting up MQTT server");
    client.setServer(mqtt_broker, 1883);
    Serial.println("MQTT server set");
    Serial.println("Setting callbacks");
    client.setCallback(handleMQTTCommands);  // Set callback once here
    Serial.println("Callbacks set");
    reconnectMQTT();
}

void reconnectMQTT() {
    Serial.println("Connecting to MQTT broker...");
    while (!client.connected()) {
        Serial.println("Attempting MQTT connection...");
        if (client.connect("KoprovBoogiemanDropper",
                        status_topic_state, 1, true, "offline")) {
            Serial.println("Subscribing to command topic");
            client.subscribe("koprov/boogieman/dropper/CMD/#");
            Serial.println("Publishing initial status");
            client.publish(status_topic_state, "online", true);

            // --- Publish local IP ---
            IPAddress ip = WiFi.localIP();
            char ipBuf[32];
            snprintf(ipBuf, sizeof(ipBuf), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
            client.publish("koprov/boogieman/dropper/status/ip", ipBuf, true);
            Serial.printf("Published IP address: %s\n", ipBuf);

            // Publish mode and threshold
            client.publish(status_topic_mode, autoMode ? "auto" : "manual", true);
            char thrBuf[16];
            snprintf(thrBuf, sizeof(thrBuf), "%d", detectionThreshold);
            client.publish(status_topic_threshold, thrBuf, true);

            publishStatusSummary();
            Serial.println("Connected to MQTT broker");
        } else {
            delay(2000);
        }
    }
}


void mqttLoop() {
    if (!client.connected()) {
        reconnectMQTT();
    }
    client.loop();
}

void handleMQTTCommands(char* topic, byte* payload, unsigned int length) {
    char buf[256];
    length = min(length, (unsigned int)sizeof(buf) - 1);
    memcpy(buf, payload, length);
    buf[length] = '\0';
    String message(buf);
    message.trim();

    String t(topic);

    // If the message was published to the base command topic, parse compact commands.
    if (t == command_topic) {
        // Support tokenized commands separated by ';' or ','
        // First token is the main command (either single word or key=value). Subsequent tokens may include id=... for request id.
        String raw = message;
        raw.trim();

        // Split tokens by ';' or ','
        const int MAX_TOKENS = 8;
        String tokens[MAX_TOKENS];
        int tokenCount = 0;
        int start = 0;
        for (int i = 0; i <= raw.length() && tokenCount < MAX_TOKENS; i++) {
            if (i == raw.length() || raw[i] == ';' || raw[i] == ',') {
                String tok = raw.substring(start, i);
                tok.trim();
                if (tok.length() > 0) tokens[tokenCount++] = tok;
                start = i + 1;
            }
        }

        String reqId = "";
        // Extract id token if present among tokens (id=...)
        for (int i = tokenCount - 1; i >= 0; --i) {
            String tk = tokens[i];
            int eq = tk.indexOf('=');
            if (eq > 0) {
                String k = tk.substring(0, eq);
                String v = tk.substring(eq + 1);
                k.trim(); v.trim();
                if (k == "id") {
                    reqId = v;
                    // remove token i by shifting
                    for (int j = i; j < tokenCount - 1; ++j) tokens[j] = tokens[j + 1];
                    tokenCount--;
                }
            }
        }

        if (tokenCount == 0) {
            publishAck(reqId, "error", "empty command");
            return;
        }

        // Process first token
        String first = tokens[0];
        first.replace(" ", "");
        int eq = first.indexOf('=');
        if (eq >= 0) {
            String key = first.substring(0, eq);
            String val = first.substring(eq + 1);
            key.trim(); val.trim();

            if (key == SET_WINDTIME_CMD) {
                float newWind = val.toFloat();
                if (newWind <= 0.0 || newWind > 60.0) {
                    publishAck(reqId, "error", "bad windTime");
                } else {
                    windSeconds = newWind;
                    char reply[64];
                    snprintf(reply, sizeof(reply), "Wind time set to %.2f", windSeconds);
                    publishAck(reqId, "ok", reply);
                    publishStatusSummary();
                }
                return;
            }

            if (key == SERVO1_CMD) {
                int us = val.toInt();
                servo1.writeMicroseconds(us);
                char reply[64];
                snprintf(reply, sizeof(reply), "servo1=%d", us);
                publishAck(reqId, "ok", reply);
                return;
            }

            if (key == SERVO2_CMD) {
                int us = val.toInt();
                servo2.writeMicroseconds(us);
                char reply[64];
                snprintf(reply, sizeof(reply), "servo2=%d", us);
                publishAck(reqId, "ok", reply);
                return;
            }

            if (key == SET_THRESHOLD_CMD) {
                int newThreshold = val.toInt();
                if (newThreshold < 5 || newThreshold > 400) {
                    publishAck(reqId, "error", "bad threshold");
                } else {
                    detectionThreshold = newThreshold;
                    char reply[64];
                    snprintf(reply, sizeof(reply), "threshold=%d", detectionThreshold);
                    publishAck(reqId, "ok", reply);
                    publishStatusSummary();
                }
                return;
            }

            if (key == SET_MODE_CMD) {
                if (val == "auto") { enterAutoMode(); publishAck(reqId, "ok", "mode=auto"); }
                else if (val == "manual") { enterManualMode(); publishAck(reqId, "ok", "mode=manual"); }
                else publishAck(reqId, "error", "bad mode");
                publishStatusSummary();
                return;
            }

            publishAck(reqId, "error", "unknown key");
            return;
        } else {
            // single word
            if (first == DROP_CMD) { drop(); publishAck(reqId, "ok", DROP_CMD); return; }
            if (first == LIFT_CMD) { lift(); publishAck(reqId, "ok", LIFT_CMD); return; }
            if (first == WINDUP_CMD) { windUp(windSeconds); publishAck(reqId, "ok", WINDUP_CMD); return; }
            if (first == RESET_CMD) { reset(); publishAck(reqId, "ok", RESET_CMD); return; }
            if (first == SLEEP_CMD) { wasAutoBeforeSleep = autoMode; if (autoMode) enterManualMode(); servo_sleep(); publishAck(reqId, "ok", SLEEP_CMD); publishStatusSummary(); return; }
            if (first == WAKEUP_CMD) { servo_wake_up(); if (wasAutoBeforeSleep) { enterAutoMode(); wasAutoBeforeSleep = false; } publishAck(reqId, "ok", WAKEUP_CMD); publishStatusSummary(); return; }
            if (first == REBOOT_CMD) { client.publish(status_topic_state, REBOOT_CMD, true); client.disconnect(); delay(200); ESP.restart(); }

            publishAck(reqId, "error", "unknown command");
            return;
        }
    }

    // Keep state update handling from other clients
    if (t == status_topic_mode) {
        if (message == "up") bmanUp = true;
        else if (message == "down") bmanUp = false;
        return;
    }

}