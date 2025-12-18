#ifndef SERVO_CONTROL_H
#define SERVO_CONTROL_H

#include "board_config.h"
#include <ESP32Servo.h>

extern Servo servo1, servo2;
const int servo1Pin = BOARD_SERVO1_PIN;
const int servo2Pin = BOARD_SERVO2_PIN;
const int UP_USEC = 1550;
const int DOWN_USEC = 500;

extern float windSeconds;

void setupServos();
void lift();
void drop();
void windUp(float seconds);
void reset();
void enterAutoMode();
void enterManualMode();
void servo_sleep();
void servo_wake_up();

#endif
