#include <BluetoothSerial.h>
#include <ESP32Servo.h>

BluetoothSerial SerialBT;

// Servo setup
Servo servo1, servo2;

// Pin definitions
const int servo1Pin = 18;   // Pin for Servo 1 control
const int servo2Pin = 19;   // Pin for Servo 2 control
const int ledPin = 2;
const int trigPin = 22;  // Trig pin for HC-SR04
const int echoPin = 23;  // Echo pin for HC-SR04

const int UP_USEC = 1550;
const int DOWN_USEC = 500;
const float IN_SEC = 2.1;
bool bmanUp = false;
bool autoMode = false;
String input;

// Function to measure distance using HC-SR04
long measureDistance() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  
  long duration = pulseIn(echoPin, HIGH);
  long distance = duration * 0.034 / 2;
  
  // Filter out invalid and too-large readings
  if (distance <= 0 || distance > 300) {
    return 9999;  // Return a large value when distance is invalid or out of range
  }
  
  return distance;
}

// Function to take multiple readings and average them for smoothing
long getSmoothedDistance(int numReadings) {
  long totalDistance = 0;
  for (int i = 0; i < numReadings; i++) {
    totalDistance += measureDistance();
    delay(50);  // Small delay between readings
  }
  return totalDistance / numReadings;  // Return the average distance
}

// Wind-up function
void windUp(float inch) {
  float sec = inch / IN_SEC;
  int totalSeconds = floor(sec);

  SerialBT.println("Start winding");
  servo2.writeMicroseconds(2500);  // Start continuous rotation for Servo 2

  for (int i = totalSeconds; i > 0; i--) {
    digitalWrite(ledPin, HIGH);
    SerialBT.print("Countdown: ");
    SerialBT.print(i);
    SerialBT.println(" seconds remaining");
    delay(500);
    digitalWrite(ledPin, LOW);
    delay(500);
  }

  SerialBT.println("Stopping winding");
  servo2.writeMicroseconds(1500);  // Stop Servo 2 (neutral position)
}

// Drop function
void drop() {
  servo1.writeMicroseconds(DOWN_USEC);
  SerialBT.println("Dropping Boogieman");
  delay(1000);
}

// Lift function
void lift() {
  servo1.writeMicroseconds(UP_USEC);
  SerialBT.println("Lifting Boogieman");
  delay(1000);
}

// Automatic mode function
void autoModeFunction() {
  SerialBT.println("Entering Auto Mode.");
  windUp(42);  // Wind up initially
  bmanUp = true;

  bool objectDetected = false;
  
  while (autoMode) {
    long distance = getSmoothedDistance(5);  // Get smoothed distance
    SerialBT.print("Smoothed Distance: ");
    SerialBT.println(distance);

    // Step 1: Detect object within 100 cm
    if (distance < 100) {
      SerialBT.println("Object detected within 100 cm.");
      objectDetected = true;
    }

    // Step 2: Wait until object clears (distance > 100 cm)
    if (objectDetected && distance >= 100) {
      SerialBT.println("Object cleared. Starting 3-second countdown...");

      unsigned long clearTime = millis();
      bool objectStillAbsent = true;

      // Step 3: 3-second countdown after object clears
      while (millis() - clearTime < 3000) {
        distance = getSmoothedDistance(5);  // Get smoothed distance during wait
        SerialBT.print("Distance during countdown: ");
        SerialBT.println(distance);

        // If object is detected again, cancel the drop
        if (distance < 100) {
          objectStillAbsent = false;
          SerialBT.println("Object detected again during countdown. Canceling drop.");
          break;
        }

        delay(200);  // Check every 200ms
      }

      // If the object is still absent after 3 seconds, drop the boogieman
      if (objectStillAbsent) {
        SerialBT.println("Dropping Boogieman after 3-second countdown.");
        drop();
        bmanUp = false;
        objectDetected = false;  // Reset the flag for future detections

        // Step 4: Wait for the area to clear for 10 seconds before winding up again
        unsigned long clearStart = millis();
        bool objectStillPresentAfterDrop = true;
        
        while (millis() - clearStart < 10000) {
          distance = getSmoothedDistance(5);  // Get smoothed distance
          SerialBT.print("Distance during clearing: ");
          SerialBT.println(distance);
          
          // If the object is detected again within 100 cm, reset the timer
          if (distance < 100) {
            clearStart = millis();
            objectStillPresentAfterDrop = true;
          } else {
            objectStillPresentAfterDrop = false;
          }

          delay(500);  // Delay between distance checks
        }

        // If the area is clear for 10 seconds, lift and wind up the boogieman
        if (!objectStillPresentAfterDrop) {
          SerialBT.println("Area clear. Lifting Boogieman.");
          lift();
          windUp(42);
          bmanUp = true;
        }
      }
    }
    
    delay(500);  // Small delay between sensor checks
  }
}

void manualMode() {
  SerialBT.println("Entering Manual Mode.");
  while (!autoMode) {
    if (SerialBT.available()) {
      SerialBT.println("Up (1) or down (2)? ");
      while (SerialBT.available() == 0) {}
      input = SerialBT.readStringUntil('\n');
      input.trim();
      int cmd = input.toInt();

      if (cmd == 1 && !bmanUp) {
        lift();
        windUp(42);  // Wind up 42 inches
        bmanUp = true;
        digitalWrite(ledPin, HIGH);
      } else if (cmd == 2) {
        drop();
        bmanUp = false;
        digitalWrite(ledPin, LOW);
      } else {
        SerialBT.println("Invalid command. Enter 1 or 2.");
      }
    }
  }
}

void setup() {
  pinMode(ledPin, OUTPUT);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  
  SerialBT.begin("Boggieman");
  SerialBT.println("Bluetooth device started. Ready for commands.");
  
  servo1.attach(servo1Pin);
  servo2.attach(servo2Pin);
  
  servo1.writeMicroseconds(550);   // Initialize to drop position
  delay(1000);
  servo1.writeMicroseconds(1500);  // Move to neutral position
  servo2.writeMicroseconds(1500);  // Stop continuous rotation servo
  delay(1000);
}

void loop() {
  if (SerialBT.available()) {
    SerialBT.println("Auto (1) or Manual (2) mode?");
    
    // Wait for user to choose mode
    while (SerialBT.available() == 0) {}
    input = SerialBT.readStringUntil('\n');
    input.trim();
    int mode = input.toInt();
    
    if (mode == 1) {
      autoMode = true;
      autoModeFunction();  // Start automatic mode
    } else if (mode == 2) {
      autoMode = false;
      manualMode();  // Enter manual mode
    } else {
      SerialBT.println("Invalid mode. Enter 1 for Auto or 2 for Manual.");
    }
  }
}
