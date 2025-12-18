#ifndef ULTRASONIC_SENSOR_H
#define ULTRASONIC_SENSOR_H


#include "board_config.h"

const int trigPin = BOARD_TRIG_PIN;
const int echoPin = BOARD_ECHO_PIN;

void setupSensor();
long getSmoothedDistance(int numReadings);

#endif
