// battery.h

#ifndef BATTERY_H
#define BATTERY_H

#include <Arduino.h>

float readVBAT(void);
uint8_t mvToPercent(float mvolts);
void checkBattery(void);

// New function declaration
uint8_t getBatteryPercentage(void);

#endif // BATTERY_H
