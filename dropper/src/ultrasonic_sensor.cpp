#include "ultrasonic_sensor.h"
#include "MQTT_config.h" // Include MQTT configuration for publishing

void setupSensor() {
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
}

long measureDistance() {
  // Ensure trigger line is stable before starting a pulse
  digitalWrite(trigPin, LOW);
  delayMicroseconds(5);

  // Send a 10us pulse to trigger the sensor
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Wait for echo with a timeout (30ms -> ~500 cm max)
  unsigned long duration = pulseIn(echoPin, HIGH, 30000UL);

  // pulseIn returns 0 on timeout
  if (duration == 0) {
    return 9999; // Invalid or out of range
  }

  long distance = static_cast<long>(duration * 0.034 / 2);

  if (distance <= 0 || distance > 400) {
    return 9999; // Invalid or out of range
  }
  return distance;
}

long getSmoothedDistance(int numReadings) {
  // Collect valid readings and compute a median value to reject spurious outliers
  const int maxReadings = max(1, numReadings);
  long readings[maxReadings];
  int validCount = 0;

  for (int i = 0; i < maxReadings; i++) {
    long d = measureDistance();
    if (d != 9999) {
      readings[validCount++] = d;
    }
    delay(50);
  }

  if (validCount == 0) {
    Serial.println("Distance: invalid");
    return 9999;
  }

  // Simple insertion sort for small arrays, then pick median
  for (int i = 1; i < validCount; i++) {
    long key = readings[i];
    int j = i - 1;
    while (j >= 0 && readings[j] > key) {
      readings[j + 1] = readings[j];
      j--;
    }
    readings[j + 1] = key;
  }

  long median;
  if (validCount % 2 == 1) {
    median = readings[validCount / 2];
  } else {
    median = (readings[validCount / 2 - 1] + readings[validCount / 2]) / 2;
  }

  Serial.println("Distance: " + String(median) + " cm");
  return median;
}
