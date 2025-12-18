#include "MQTT_config.h"
#include "servo_control.h"
#include "ultrasonic_sensor.h"
#include "server.h"
#include <ESPmDNS.h>


void setup() {
  Serial.begin(115200);
  delay(200);

  setupWiFi();  // Ensure this is called before anything else that requires network
  MDNS.begin("dropper");   // visit http://dropper.local
  setupMQTT();
  setupServos();
  setupSensor();
  setupWebServer(&autoMode);  // Pass pointer to autoMode flag for web server control

}

void loop() {
  webServerLoop();

  static unsigned long lastSample = 0;
  const unsigned long sampleInterval = 1000; // 1 second
  const unsigned long sleepCheckInterval = 2000; // 2 seconds

  // Guard to avoid publishing repeated drop commands while object remains detected
  static bool dropPublished = false;

  // Always keep MQTT processing running so commands are received
  mqttLoop();

  unsigned long now = millis();

  // If servos/sensor are asleep, skip distance sampling and only check for wake-up
  if (!servoAwake) {
    static unsigned long lastSleepCheck = 0;
    if (now - lastSleepCheck >= sleepCheckInterval) {
      lastSleepCheck = now;
      // Nothing else required here; wake command will be handled via MQTT callback which sets servoAwake
    }
    return;
  }

  if (now - lastSample >= sampleInterval) {
    lastSample = now;
    // Only sample distance in automatic mode. Manual mode does not need distance updates.
    if (autoMode) {
      long distance = getSmoothedDistance(5);

      // Skip invalid/out-of-range readings
      if (distance == 9999) {
        Serial.println("Ultrasonic: invalid reading, skipping publish");
      } else {
        // Publish distance for monitoring (retained)
        char buf[16];
        snprintf(buf, sizeof(buf), "%ld", distance);
        client.publish(status_topic_distance, buf, true);

        // Automatic mode: publish a single drop command when threshold crossed
        if (distance < detectionThreshold && !dropPublished) {
            client.publish(command_topic, "drop", true);
            dropPublished = true;
          } else if (distance >= detectionThreshold && dropPublished) {
          // Reset guard once object clears so next detection triggers again
          dropPublished = false;
        }
      }
    } else {
      // In manual mode we don't sample distance; ensure drop guard is reset
      dropPublished = false;
    }
  }
}
