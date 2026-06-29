#ifndef DISPLAY_H
#define DISPLAY_H

#include "settings.h"
#include <GxEPD2_BW.h>

extern GxEPD2_BW<GxEPD2_150_BN, GxEPD2_150_BN::HEIGHT>* display;

extern int disp_top_margin;
extern int disp_bottom_margin;
extern int disp_right_margin;
extern int disp_font_height;
extern int disp_icon_height;
extern int disp_icon_width;
extern int disp_window_offset;
extern int disp_height;
extern int disp_width;

extern const uint16_t off_icon[16];

void setupDisplay();
void drawModeIcon(const char* mode);
void drawIcon(const uint16_t* icon_data, int x, int y, int height, int width, uint16_t bg_color, uint16_t icon_color);
void enableBacklight(bool en);
void showError(const char* error_msg);
void clearScreen();

// Battery icons — extern for layout module to reference
extern const uint16_t bat100_icon[16];
extern const uint16_t bat80_icon[16];
extern const uint16_t bat60_icon[16];
extern const uint16_t bat40_icon[16];
extern const uint16_t bat20_icon[16];
extern const uint16_t bat10_icon[16];
extern const uint16_t bat0_icon[16];

// Stubs — rendering moved to display_layout; definitions in display.cpp keep callers linking
void updDisp(uint8_t line, const char* msg, bool updateScreen = true);
void updModeAndChannelDisplay();
void printStatusIcons();
void printGPSIcon();
void printFrequencyIcon(bool updateScreen = false);
void printTimeIcon(bool updateScreen = false);
void printStatusOnApp();
void sleepDisplay();

#endif
