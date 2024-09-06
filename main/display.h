#ifndef DISPLAY_H
#define DISPLAY_H

#include "settings.h"
#include <GxEPD2_BW.h>  // Include GxEPD2 library for b/w displays

// Define the display object for the specific e-paper display you're using (GxEPD2_150_BN for 1.54" b/w display)
extern GxEPD2_BW<GxEPD2_150_BN, GxEPD2_150_BN::HEIGHT>* display;

extern int disp_top_margin;
extern int disp_font_height;
extern int disp_window_offset;
extern int disp_height;
extern int disp_width;

void setupDisplay();
void drawModeIcon(OperationMode mode);
void updDisp(uint8_t line, const char* msg, bool updateScreen = true);
void updModeAndChannelDisplay();
void enableBacklight(bool en);
void showError(const char* error_msg);
void clearScreen();

#endif
