#include "MQTT_config.h"
#include "servo_control.h"
#include "ultrasonic_sensor.h"

void setup() {
  setupWiFi();  // Ensure this is called before anything else that requires network
  setupServos();
  setupSensor();
  setupMQTT();
}

void loop() {
  mqttLoop();  // This will handle incoming MQTT messages and automatically call handleMQTTCommands when needed
}
