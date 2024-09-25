#ifndef DISPLAY_H
#define DISPLAY_H

#include <U8g2lib.h>
#include <Arduino.h>

// Function to initialize the display
void initDisplay();

// Function to update the screen with device info
void displayInfo(const String& deviceName, const String& deviceAddr, const String& rawCod, const String& deviceClass, const char* incomingChar = nullptr);

#endif  // DISPLAY_H
