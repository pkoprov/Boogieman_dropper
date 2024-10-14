#ifndef ULTRASONIC_SENSOR_H
#define ULTRASONIC_SENSOR_H

void setupSensor();
long getSmoothedDistance(int numReadings);

#endif
