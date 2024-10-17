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
  client.publish(state_topic, "up", true);
  delay(1000);
}

void drop() {
  servo1.writeMicroseconds(DOWN_USEC);
  client.publish(state_topic, "down", true);
  startScreamer();  // Start the screamer when the boogieman drops
  delay(1000);
  bmanUp = false;
}

void windUp(float inch) {
  float sec = inch / 2.1;
  int totalSeconds = floor(sec);

  client.publish(windup_status, "Start winding", true);
  servo2.writeMicroseconds(2500); // Start continuous rotation for Servo 2

  for (int i = totalSeconds; i > 0; i--) {
    digitalWrite(2, HIGH);
    delay(500);
    digitalWrite(2, LOW);
    delay(500);
  }

  servo2.writeMicroseconds(1500); // Stop Servo 2 (neutral position)
  client.publish(windup_status, "Stopping winding", true);
  bmanUp = true;
}
}

void enterAutoMode() {
    autoMode = true;
    // Notify the broker that auto mode is active
    client.publish(mode_topic, "auto", true);
    drop();
    stopScreamer();
    lift();
    windUp(20);
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
            client.publish(auto_distance, "Object detected within 100 cm", true);
            objectDetected = true;
        }

        // Step 2: Wait until object clears (distance > 100 cm)
        if (objectDetected && distance >= 100) {
            client.publish(auto_distance, "Object cleared. Starting 3-second countdown...", true);

            unsigned long clearTime = millis();
            bool objectStillAbsent = true;

            // Step 3: 3-second countdown after object clears
            while (millis() - clearTime < 3000) {
                distance = getSmoothedDistance(5);  // Check distance during wait

                if (distance < DETECTION_THRESHOLD) {
                    objectStillAbsent = false;
                    client.publish(auto_distance, "Object detected again during countdown. Canceling drop.", true);
                    break;
                }
                delay(200);  // Check every 200ms
            }

            // If the object is still absent after 3 seconds, drop the boogieman
            if (objectStillAbsent) {
                client.publish(auto_distance, "Dropping Boogieman after 3-second countdown.", true);
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
                stopScreamer();

                // If the area is clear for 10 seconds, lift and wind up the boogieman
                if (!objectStillPresentAfterDrop) {
                    client.publish(auto_distance, "Area clear. Lifting Boogieman.", true);
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
    autoMode = false;
    // Publish to MQTT that manual mode is active
    client.publish(mode_topic, "manual", true);
    
    // Publish the initial state (up or down)
    client.publish(state_topic, bmanUp ? "up" : "down", true);

    // Loop to remain in manual mode until switched to auto mode
    while (!autoMode) {
        // All the manual command handling (up/down) happens in handleMQTTCommands()
        mqttLoop();  // Process incoming MQTT messages
        delay(100);  // Small delay to avoid excessive loop running
    }
}

