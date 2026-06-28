#pragma once

#include <Arduino.h>
#include <GxEPD2_BW.h>

// Access the display object defined in display.cpp
extern GxEPD2_BW<GxEPD2_150_BN, GxEPD2_150_BN::HEIGHT>* display;

void showBootAnimation(bool lora_ok, bool gps_ok);
void showBootLogo();
