#ifndef SERVO_CONTROL_H
#define SERVO_CONTROL_H

#include <ESP32Servo.h>

void setupServos();
void lift();
void drop();
void windUp(float inch);
void enterAutoMode();
void enterManualMode();

#endif
