#ifndef MQTT_CONFIG_H
#define MQTT_CONFIG_H

#include <PubSubClient.h>
#include <WiFi.h>

extern const char* mqtt_broker;
extern const char* status_topic;
extern const char* command_topic;
extern const char* mode_topic;
extern const char* state_topic;
extern const char* manual_command_topic;

extern PubSubClient client;

void setupWiFi();
void setupMQTT();
void mqttLoop();
void handleMQTTCommands(char* topic, byte* payload, unsigned int length);
void reconnectMQTT();

#endif
