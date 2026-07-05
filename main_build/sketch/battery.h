#line 1 "V:\\Bmad\\Project_ptt_lora\\main\\battery.h"
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
