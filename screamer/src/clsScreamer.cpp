#include <WiFi.h>
#include <PubSubClient.h>
#include "clsScreamer.h"

WiFiClient espClient;
PubSubClient client(espClient);
const char* mqtt_broker = "broker.hivemq.com";
const char* command_topic = "koprov/boogieman/screamer/CMD";
const char* status_topic = "koprov/boogieman/screamer/status";
const char* state_topic = "koprov/boogieman/screamer/state";
const int outputPin = 5;
const int ledPin = 2;

// Static member initialization
clsScreamer* clsScreamer::instance = nullptr;

clsScreamer::clsScreamer() {
    // Set the singleton instance
    instance = this;
}

void clsScreamer::setupScreamer() {
    pinMode(outputPin, OUTPUT);
    pinMode(ledPin, OUTPUT);
    digitalWrite(outputPin, LOW);
    digitalWrite(ledPin, LOW);
}

void clsScreamer::initializeMQTT() {
    client.setServer(mqtt_broker, 1883);
    client.setCallback(clsScreamer::mqttCallbackWrapper);
    reconnectMQTT();
}

void clsScreamer::handleScreamer() {
    if (!client.connected()) {
        reconnectMQTT();
    }
    client.loop();
}

void clsScreamer::reconnectMQTT() {
    if (!client.connected()) {
        Serial.print("Attempting MQTT reconnection...");
        // Fixed client ID as specified
        if (client.connect("KoprovBoogiemanScreamer", NULL, NULL, status_topic, 1, true, "offline")) {
            Serial.println("connected");
            client.publish(status_topic, "online", true);  // Report online status
            client.subscribe(command_topic);
        } else {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            delay(5000);
        }
    }
}

void clsScreamer::mqttCallbackWrapper(char* topic, byte* message, unsigned int length) {
    if (instance) {
        instance->mqttCallback(topic, message, length);
    }
}

void clsScreamer::mqttCallback(char* topic, byte* message, unsigned int length) {
    blinkLed();
    String messageTemp;
    for (int i = 0; i < length; i++) {
        messageTemp += (char)message[i];
    }

    if (messageTemp == "1") {
        digitalWrite(outputPin, HIGH);
        digitalWrite(ledPin, HIGH);
        client.publish(state_topic, "scream", true);
        delay(100);
        digitalWrite(ledPin, LOW);
    } else if (messageTemp == "0") {
        digitalWrite(outputPin, LOW);
        digitalWrite(ledPin, LOW);
        client.publish(state_topic, "mute", true);
    }
}

void clsScreamer::blinkLed() {
    for (int i = 0; i < 2; i++) {
        digitalWrite(ledPin, HIGH);
        delay(100);
        digitalWrite(ledPin, LOW);
        delay(100);
    }
}
