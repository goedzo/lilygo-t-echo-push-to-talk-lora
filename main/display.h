#ifndef DISPLAY_H
#define DISPLAY_H

#include "settings.h"
#include <GxEPD2_BW.h>

void setupDisplay();
void drawModeIcon(OperationMode mode);
void updDisp(uint8_t line, const char* msg);
void updateMessageDisplay();
void updModeAndChannelDisplay();
void enableBacklight(bool en);

#endif