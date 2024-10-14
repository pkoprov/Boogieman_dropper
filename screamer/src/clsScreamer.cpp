#include <WiFi.h>
#include <PubSubClient.h>
#include "clsScreamer.h"

WiFiClient espClient;
PubSubClient client(espClient);
const char* mqtt_broker = "broker.hivemq.com";
const char* command_topic = "koprov/boogieman/screamer/CMD";
const char* status_topic = "koprov/boogieman/screamer/status";
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
    while (!client.connected()) {
        String clientId = "ESP32Client-";
        clientId += String(random(0xffff), HEX);
        // Use the correct method to set the Will message during the connection attempt
        if (client.connect(clientId.c_str(), NULL, NULL, status_topic, 0, true, "offline", true)) {  // Last parameter is for clean session
            client.subscribe(command_topic);
            client.publish(status_topic, "online", true);  // Report online status
        } else {
            Serial.println("MQTT connection failed. Attempting to reconnect...");
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
    String messageTemp;
    for (int i = 0; i < length; i++) {
        messageTemp += (char)message[i];
    }

    if (messageTemp == "1") {
        digitalWrite(outputPin, HIGH);
        blinkLed();
    } else if (messageTemp == "0") {
        digitalWrite(outputPin, LOW);
        digitalWrite(ledPin, LOW);
    }
}

void clsScreamer::blinkLed() {
    for (int i = 0; i < 5; i++) {
        digitalWrite(ledPin, HIGH);
        delay(200);
        digitalWrite(ledPin, LOW);
        delay(200);
    }
}
