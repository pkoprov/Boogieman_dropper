// Centralized board pin configuration.
// Edit or override these defines when porting to a different board (for example XIAO ESP32-C3).

#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

// Default pins for ESP32 Dev Module (change for XIAO ESP32-C3)
// Trig/Echo for HC-SR04
// #ifndef BOARD_TRIG_PIN
// #define BOARD_TRIG_PIN 32
// #endif

// #ifndef BOARD_ECHO_PIN
// #define BOARD_ECHO_PIN 35
// #endif

// // Servo pins
// #ifndef BOARD_SERVO1_PIN
// #define BOARD_SERVO1_PIN 15
// #endif

// #ifndef BOARD_SERVO2_PIN
// #define BOARD_SERVO2_PIN 2
// #endif

// If you're porting to XIAO ESP32-C3, pick pins that exist and support the required mode.
// Example (uncomment and adjust to your board's pinout):

// XIAO ESP32-C3 example (adjust after checking your board schematic):
#undef BOARD_TRIG_PIN
#define BOARD_TRIG_PIN 21

#undef BOARD_ECHO_PIN
#define BOARD_ECHO_PIN 7

#undef BOARD_SERVO1_PIN
#define BOARD_SERVO1_PIN 8

#undef BOARD_SERVO2_PIN
#define BOARD_SERVO2_PIN 20


#endif // BOARD_CONFIG_H
