#include "servo_control.h"
#include "ultrasonic_sensor.h"
#include "MQTT_config.h"

Servo servo1, servo2;
const int servo1Pin = 18;
const int servo2Pin = 19;
const int UP_USEC = 1550;
const int DOWN_USEC = 500;
bool bmanUp = false;
bool autoMode = false;  
const int DETECTION_THRESHOLD = 100;  // cm

void setupServos() {
  servo1.attach(servo1Pin);
  servo2.attach(servo2Pin);

  servo1.writeMicroseconds(1500); // Neutral position
  servo2.writeMicroseconds(1500); // Stop rotation
}

void lift() {
  servo1.writeMicroseconds(UP_USEC);
  client.publish(manual_state_topic, "up", true);
  delay(1000);
}

void drop() {
  servo1.writeMicroseconds(DOWN_USEC);
  client.publish(manual_state_topic, "down", true);
  delay(1000);
}

void windUp(float inch) {
  float sec = inch / 2.1;
  int totalSeconds = floor(sec);

  client.publish("koprov/boogieman/dropper/windUpStatus", "Start winding", true);
  servo2.writeMicroseconds(2500); // Start continuous rotation for Servo 2

  for (int i = totalSeconds; i > 0; i--) {
    digitalWrite(2, HIGH);
    delay(500);
    digitalWrite(2, LOW);
    delay(500);
  }

  servo2.writeMicroseconds(1500); // Stop Servo 2 (neutral position)
  client.publish("koprov/boogieman/dropper/windUpStatus", "Stopping winding", true);
}

void enterAutoMode() {
    client.publish(mode_topic, "auto", true); // Notify the broker that auto mode is active
    windUp(42);  // Wind up initially
    bmanUp = true;

    bool objectDetected = false;

    while (autoMode) {
        long distance = getSmoothedDistance(5);  // Get smoothed distance

        // Publish distance to MQTT for monitoring
        char distanceStr[10];
        snprintf(distanceStr, sizeof(distanceStr), "%ld", distance);
        client.publish("koprov/boogieman/dropper/auto/distance", distanceStr);

        // Step 1: Detect object within 100 cm
        if (distance < DETECTION_THRESHOLD) {
            client.publish("koprov/boogieman/dropper/auto/status", "Object detected within 100 cm", true);
            objectDetected = true;
        }

        // Step 2: Wait until object clears (distance > 100 cm)
        if (objectDetected && distance >= 100) {
            client.publish("koprov/boogieman/dropper/auto/status", "Object cleared. Starting 3-second countdown...", true);

            unsigned long clearTime = millis();
            bool objectStillAbsent = true;

            // Step 3: 3-second countdown after object clears
            while (millis() - clearTime < 3000) {
                distance = getSmoothedDistance(5);  // Check distance during wait

                if (distance < DETECTION_THRESHOLD) {
                    objectStillAbsent = false;
                    client.publish("koprov/boogieman/dropper/auto/status", "Object detected again during countdown. Canceling drop.", true);
                    break;
                }
                delay(200);  // Check every 200ms
            }

            // If the object is still absent after 3 seconds, drop the boogieman
            if (objectStillAbsent) {
                client.publish("koprov/boogieman/dropper/auto/status", "Dropping Boogieman after 3-second countdown.", true);
                drop();
                bmanUp = false;
                objectDetected = false;

                // Step 4: Wait for the area to clear for 10 seconds before winding up again
                unsigned long clearStart = millis();
                bool objectStillPresentAfterDrop = true;

                while (millis() - clearStart < 10000) {
                    distance = getSmoothedDistance(5);

                    if (distance < DETECTION_THRESHOLD) {
                        clearStart = millis();  // Reset the timer
                        objectStillPresentAfterDrop = true;
                    } else {
                        objectStillPresentAfterDrop = false;
                    }

                    delay(500);  // Delay between checks
                }

                // If the area is clear for 10 seconds, lift and wind up the boogieman
                if (!objectStillPresentAfterDrop) {
                    client.publish("koprov/boogieman/dropper/auto/status", "Area clear. Lifting Boogieman.", true);
                    lift();
                    windUp(42);
                    bmanUp = true;
                }
            }
        }
        delay(500);  // Delay between sensor checks
    }
}


void enterManualMode() {
    client.publish(mode_topic, "manual", true); // Notify the broker that manual mode is active
    client.publish(manual_state_topic, bmanUp ? "up" : "down", true);  // Publish the initial state (up or down)

    while (!autoMode) {  // Manual mode loop until auto mode is triggered
        // MQTT will receive commands via the `manual_command_topic` (koprov/boogieman/dropper/manual/CMD)
        client.setCallback([](char* topic, byte* payload, unsigned int length) {
            String input = String((char*)payload).substring(0, length);  // Parse the MQTT message
            input.trim();  // Remove leading/trailing whitespace

            if (input == "up" && !bmanUp) {
                lift();
                windUp(42);  // Wind up 42 inches
                bmanUp = true;
                client.publish(manual_state_topic, "up", true);
            } else if (input == "drop") {
                drop();
                bmanUp = false;
                client.publish(manual_state_topic, "down", true);
            } else {
                client.publish("koprov/boogieman/dropper/manual/error", "Invalid command. Use 'up' or 'drop'", true);
            }
        });

        mqttLoop();  // Ensure MQTT messages are being processed
        delay(100);  // Small delay to avoid excessive loop running
    }
}
