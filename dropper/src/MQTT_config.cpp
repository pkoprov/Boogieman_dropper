#include "MQTT_config.h"
#include "servo_control.h"

const char* mqtt_broker = "broker.hivemq.com";
const char* status_topic = "koprov/boogieman/dropper/status";
const char* command_topic = "koprov/boogieman/dropper/CMD";
const char* mode_topic = "koprov/boogieman/dropper/mode";
const char* state_topic = "koprov/boogieman/dropper/manual/state";
const char* manual_command_topic = "koprov/boogieman/dropper/CMD/manual";
const char* auto_distance =  "koprov/boogieman/dropper/auto/distance";
const char* windup_status = "koprov/boogieman/dropper/windUpStatus";

WiFiClient espClient;
PubSubClient client(espClient);

void setupWiFi() {
    WiFi.begin(ssid, password);
    Serial.println("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.println("Connected to WiFi");
}

void setupMQTT() {
    client.setServer(mqtt_broker, 1883);
    client.setCallback(handleMQTTCommands);  // Set callback once here
    reconnectMQTT();
}

void reconnectMQTT() {
  while (!client.connected()) {
    if (client.connect("KoprovBoogiemanDropper", NULL, NULL, status_topic, 1, true, "offline")) {
      client.publish(status_topic, "online", true);
      client.subscribe(command_topic);
      client.subscribe(manual_command_topic);
            client.subscribe(state_topic);
    } else {
      delay(5000);
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
    payload[length] = '\0'; // Ensure null-terminated string
    String message = String((char*)payload);

    // Handle mode switching (auto/manual)
    if (String(topic) == command_topic) {
        if (message == "auto") {
            enterAutoMode();
        } else if (message == "manual") {
            enterManualMode();  // No need to publish inside handleMQTTCommands
        }
    }

    // Handle manual commands (up/down)
    else if (String(topic) == manual_command_topic && !autoMode) {
        if (message == "up") {
            stopScreamer();  // Stop the screamer right before winding up
            lift();
            windUp(42);  // Example distance
            client.publish(state_topic, "up", true);
        } else if (message == "drop") {
            drop();
            client.publish(state_topic, "down", true);
        } else if (String(topic) == state_topic) {
            if (message == "up") bmanUp = true;
            else if (message == "down") bmanUp = false;
        } else {
            client.publish("koprov/boogieman/dropper/errors", "Invalid command. Use 'up' or 'drop'", true);
        }
    }
}

void startScreamer() {
    client.publish("koprov/boogieman/screamer/CMD", "1", true);  // Start the screamer
}

void stopScreamer() {
    client.publish("koprov/boogieman/screamer/CMD", "0", true);  // Stop the screamer
}