#ifndef MQTT_CONFIG_H
#define MQTT_CONFIG_H

#define DROP_CMD  "drop"
#define LIFT_CMD "up"
#define WINDUP_CMD "windUp"
#define RESET_CMD "reset"
#define SLEEP_CMD "sleep"
#define WAKEUP_CMD "wakeUp"
#define REBOOT_CMD "reboot"
#define SET_THRESHOLD_CMD "threshold"
#define SET_MODE_CMD "mode"
#define SET_WINDTIME_CMD "windTime"
#define SERVO1_CMD "servo1"
#define SERVO2_CMD "servo2"

#include <PubSubClient.h>
#include <WiFi.h>

extern bool autoMode;
extern bool bmanUp;
extern bool servoAwake;

// Configurable WiFi/MQTT parameters.
// These are defined in the header using C++17 `inline` variables so they can be
// declared+defined here without producing multiple-definition linker errors.
// Edit the values below to customize the build. If your toolchain does not
// support inline variables, replace `inline` with `static` (internal linkage)
// or move the definitions to a single .cpp file.
inline const char* wifi_ssid = "SSID";        // change to your WiFi SSID
inline const char* wifi_password = "password"; // change to your WiFi password

// MQTT broker address (IP or hostname)
inline const char* mqtt_broker = "192.168.0.0";

inline const char* status_topic_mode = "koprov/boogieman/dropper/status/mode";
inline const char* command_topic = "koprov/boogieman/dropper/CMD"; // base command topic: koprov/boogieman/dropper/CMD
inline const char* status_reply_topic = "koprov/boogieman/dropper/status/reply";
inline const char* status_result_topic = "koprov/boogieman/dropper/status/result";
inline const char* status_topic_servo = "koprov/boogieman/dropper/status/servo";
inline const char* status_topic_windUp = "koprov/boogieman/dropper/status/windUp";
inline const char* status_topic_windTime = "koprov/boogieman/dropper/status/windTime";
inline const char* status_topic_threshold = "koprov/boogieman/dropper/status/dist_threshold";
inline const char* status_topic_state = "koprov/boogieman/dropper/status/state";
inline const char* status_topic_distance =  "koprov/boogieman/dropper/status/distance";

extern PubSubClient client;

// Detection threshold default (can be changed via MQTT)
inline int detectionThreshold = 50; // cm

void setupWiFi();
void setupMQTT();
void mqttLoop();
void handleMQTTCommands(char* topic, byte* payload, unsigned int length);
void reconnectMQTT();
void publishStatusSummary();

#endif
