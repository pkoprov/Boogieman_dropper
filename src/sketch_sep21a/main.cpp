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

void drop() {
  servo1.writeMicroseconds(DOWN_USEC);
  SerialBT.println("Dropping Boogieman");
  delay(1000);
}

void lift() {
  servo1.writeMicroseconds(UP_USEC);
  SerialBT.println("Lifting Boogieman");
  delay(1000);
}

void setup() {
  pinMode(ledPin, OUTPUT);
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

String input;

void loop() {
  if (SerialBT.available()) {
    SerialBT.println("Up (1) or down (2)? ");
    while (SerialBT.available() == 0) {}
    input = SerialBT.readStringUntil('\n');  // Read until newline
    input.trim();  // Remove any whitespace or newlines
    int cmd = input.toInt();

    if (cmd == 1 && !bmanUp)
      {
        lift();
        windUp(42);
        bmanUp = true;
        digitalWrite(ledPin, HIGH);
      }
    else if (cmd == 2)
      {
        drop();
        bmanUp = false;
        digitalWrite(ledPin, LOW);
      }
    else SerialBT.println("Invalid command. Enter 1 or 2.");
  }
}
