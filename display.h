#ifndef DISPLAY_H
#define DISPLAY_H

#include <GxEPD2_BW.h>
#include <Fonts/FreeMonoBold9pt7b.h>

void setupDisplay();
void drawModeIcon(OperationMode mode);
void updDisp(uint8_t line, const char* msg);
void updateMessageDisplay();
void updModeAndChannelDisplay();

#endif