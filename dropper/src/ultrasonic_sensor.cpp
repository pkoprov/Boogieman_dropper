#include "ultrasonic_sensor.h"
#include "MQTT_config.h" // Include MQTT configuration for publishing

const int trigPin = 22;
const int echoPin = 23;

void setupSensor() {
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
}

long measureDistance() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH);
  long distance = duration * 0.034 / 2;

  if (distance <= 0 || distance > 400) {
    return 9999; // Invalid or out of range
  }
  return distance;
}

long getSmoothedDistance(int numReadings) {
  long totalDistance = 0;
  for (int i = 0; i < numReadings; i++) {
    totalDistance += measureDistance();
    delay(50);
  }
  return totalDistance / numReadings;
}
