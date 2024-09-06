#ifndef DISPLAY_H
#define DISPLAY_H

#include "settings.h"
#include <GxEPD2_BW.h>
#include <GxIO/GxIO_SPI/GxIO_SPI.h>
#include <GxIO/GxIO.h>
#include <GxDEPG0150BN/GxDEPG0150BN.h>  // 1.54" b/w 

extern GxEPD_Class     *display;
extern int disp_top_margin;
extern int disp_font_height;
extern int disp_window_offset;
extern int disp_window_heigth_fix;
extern int disp_height;
extern int disp_width;

void setupDisplay();
void drawModeIcon(OperationMode mode);
void updDisp(uint8_t line, const char* msg);
void updateMessageDisplay();
void updModeAndChannelDisplay();
void enableBacklight(bool en);
void showError(const char* error_msg);
#endif