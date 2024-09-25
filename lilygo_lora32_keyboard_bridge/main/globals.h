#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>

// Maximum number of devices to store
extern const int MAX_TRIED_DEVICES;

// Array to store tried devices
extern String triedDevices[];

// Counter for tried devices
extern int triedDeviceCount;

#endif  // GLOBALS_H
