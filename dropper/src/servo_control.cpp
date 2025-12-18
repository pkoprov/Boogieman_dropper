#include "servo_control.h"
#include "ultrasonic_sensor.h"
#include "MQTT_config.h"


Servo servo1, servo2;
bool bmanUp = false;
bool autoMode = false;  
// Define windSeconds here (previously in header as extern)
float windSeconds = 1;

// Track whether servo subsystem (and sensor polling) should be awake
bool servoAwake = true;
void setupServos() {
  servo1.attach(servo1Pin);
  servo2.attach(servo2Pin);

  servo1.writeMicroseconds(1500); // Neutral position
  servo2.writeMicroseconds(1500); // Stop rotation
}

void lift() {
  servo1.writeMicroseconds(UP_USEC);
  client.publish(status_topic_servo, "up", true);
  delay(1000);
}

void drop() {
  servo1.writeMicroseconds(DOWN_USEC);
  client.publish(status_topic_servo, "down", true);
  delay(1000);
  bmanUp = false;
}

void windUp(float seconds) {
    int totalSeconds = static_cast<int>(seconds);

    client.publish(status_topic_windUp, "Start winding", true);
    servo2.writeMicroseconds(2500); // Start continuous rotation for Servo 2

    delay(totalSeconds * 1000);
    servo2.writeMicroseconds(1500); // Stop Servo 2 (neutral position)
    client.publish(status_topic_windUp, "Stopping winding", true);
    bmanUp = true;
}

void reset() {
  lift();
  windUp(windSeconds);
}

void remainingTime(unsigned long clearTime, int preset) {
    int remainingTime = (preset - (millis() - clearTime)) / 1000;
    char message[50];  // Buffer to store the formatted string

    // Format the string
    sprintf(message, "restarting in %d sec", remainingTime);
    client.publish(status_topic_distance, message, true);
}

void servo_sleep() {
  servo1.writeMicroseconds(2500); // Vertical position
  delay(1000);
  windUp(1); // Wind up a bit before sleeping
  servo2.writeMicroseconds(2500);
  delay(500);
  servo2.writeMicroseconds(1500); // ensure servo2 is in expected sleep position
  servoAwake = false;
  servo2.detach();

  client.publish(status_topic_state, "sleeping", true);
  delay(200);                 // give time for packet to be sent
}

void servo_wake_up() {
    servo1.attach(servo1Pin);
    servo2.attach(servo2Pin);

    client.publish(status_topic_state, "awake", true);
    delay(200);                 // give time for packet to be sent
    drop();
    servo2.writeMicroseconds(500); // unwind a bit
    servoAwake = true;
    delay(1000);
    servo2.writeMicroseconds(1500); // stop
    delay(1000);
    lift();
}

void enterAutoMode() {
  autoMode = true;
  // Notify the broker that auto mode is active
  client.publish(status_topic_mode, "auto", true);

  bool waitingForClear = false;
  unsigned long clearStart = 0;

  // Start by ensuring boogieman is wound and up
  if (!bmanUp) {
    lift();
    windUp(windSeconds);
    bmanUp = true;
  }

  client.publish(status_topic_distance, "Auto mode engaged", true);

  while (autoMode) {
    // Always service MQTT so we remain responsive
    mqttLoop();
    webServerLoop();
    yield();

    long distance = getSmoothedDistance(5);

    // Publish distance periodically
    char distanceStr[16];
    snprintf(distanceStr, sizeof(distanceStr), "%ld", distance);
    client.publish(status_topic_distance, distanceStr, true);

    // If not currently in waiting-for-clear phase, look for an initial trigger
    if (!waitingForClear) {
  if (distance < detectionThreshold) {
        // Someone entered the zone — begin waiting for them to clear
        waitingForClear = true;
        client.publish(status_topic_distance, "Object detected - waiting for clear", true);
        // Continue loop; we'll detect when they clear
      }
    } else {
      // We were triggered previously. We're waiting for the distance to go above threshold
  if (distance >= detectionThreshold) {
        // Area cleared; start 3-second countdown
        client.publish(status_topic_distance, "Area cleared - starting 2s countdown", true);
        clearStart = millis();

        bool aborted = false;
        while (millis() - clearStart < 2000) {
          mqttLoop();
          distance = getSmoothedDistance(3);
          if (distance < detectionThreshold) {
            // Someone re-entered — abort countdown and restart waiting
            aborted = true;
            client.publish(status_topic_distance, "Object re-detected during countdown - aborting", true);
            break;
          }

          // Publish remaining time every ~500ms
          remainingTime(clearStart, 2000);
          delay(500);
        }

        if (!aborted) {
          // Still clear after 3 seconds — perform drop
          client.publish(status_topic_distance, "Clear for 2s - dropping", true);
          drop();
          bmanUp = false;

          // Wait 5 seconds with area clear requirement: if someone walks in, restart the process
          unsigned long postDropStart = millis();
          bool someoneReturned = false;
          while (millis() - postDropStart < 3000) {
            mqttLoop();
            distance = getSmoothedDistance(3);
            if (distance < detectionThreshold) {
              someoneReturned = true;
              client.publish(status_topic_distance, "Person returned after drop - restarting auto sequence", true);
              break;
            }
            delay(200);
          }

          if (!someoneReturned) {
            // Area remained clear for 3s; lift and wind after
            client.publish(status_topic_distance, "Area clear after drop - lifting and winding", true);
            lift();
            windUp(windSeconds);
            bmanUp = true;
          }

          // Reset waiting state so we look for next detection
          waitingForClear = false;
        } else {
          // Aborted countdown, keep waiting for next clear
          waitingForClear = true; // remain in triggered state
        }
      }
    }

    // Small delay to pace sensor checks
    delay(300);
  }
}


void enterManualMode() {
    autoMode = false;
    // Publish to MQTT that manual mode is active
    client.publish(status_topic_mode, "manual", true);
    
    // Publish the initial state (up or down)
    client.publish(status_topic_state, bmanUp ? "up" : "down", true);
}

