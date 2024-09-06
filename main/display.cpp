#include "utilities.h"
#include <stdint.h>
#include "settings.h"
#include "app_modes.h"
#include <GxIO/GxIO_SPI/GxIO_SPI.h>
#include <GxIO/GxIO.h>
#include <GxDEPG0150BN/GxDEPG0150BN.h>  // 1.54" b/w 
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include "display.h"

// Define the 16x16 pixel icon for "TXT" mode
const uint16_t txt_icon[16] = {
    0b0000000000000000,
    0b0000110000110000,
    0b0001111001111000,
    0b0011001111001100,
    0b0110000110000110,
    0b0111111111111110,
    0b1100000110000011,
    0b1100000110000011,
    0b0000000110000011,
    0b0001111111111110,
    0b0011000110000110,
    0b0110000110000110,
    0b0110000110000110,
    0b0111000110001110,
    0b0000000000000000,
    0b0000000000000000
};

// Define the 16x16 pixel icon for "PTT" mode
const uint16_t ptt_icon[16] = {
    0b0000000000000000,
    0b0000000000000000,
    0b0000000011111111,
    0b0000111111111111,
    0b1111111111111111,
    0b1111111111111111,
    0b1111111111111111,
    0b1111111111111111,
    0b1111111111111111,
    0b1111111111111111,
    0b0000111111111111,
    0b0000000011111111,
    0b0000000000001111,
    0b0000000000000000,
    0b0000000000000000,
    0b0000000000000000
};

// E-Paper display initialization
SPIClass        *dispPort  = nullptr;
GxIO_Class      *io        = nullptr;
GxEPD_Class     *display   = nullptr;


int disp_top_margin=14;   //The blank margin on top
int disp_font_height=14;  //The height of a line in pixels
int disp_window_offset=10; //The correction for display->UpdateWindow to print show the updated line. Moved up
int disp_window_heigth_fix=disp_font_height-3;  //Looks like the heigth for display->UpdateWindow is bigger then the font_height, so this must be corrected
int disp_height=200;      //The height of the displsy
int disp_width=200;       //The width of the display

// Display buffer for each line (Top line, middle, bottom, and errors)
char disp_buf[20][24] = {
    "",  // Top line with channel and bitrate
    "",                 // Middle line
    "",                        // Bottom line for errors
    ""                         // 4th line for non-PTT messages
};

// Buffer to store 10 lines of received text messages
char message_lines[10][24] = {""};
int current_message_line = 0;

void printline(const char* msg) {
    Serial.println(msg);
    Serial.println("\n");
    display->print(msg);
}

void setupDisplay() {

    dispPort = new SPIClass(
        /*SPIPORT*/NRF_SPIM2,
        /*MISO*/ ePaper_Miso,
        /*SCLK*/ePaper_Sclk,
        /*MOSI*/ePaper_Mosi);

    io = new GxIO_Class(
        *dispPort,
        /*CS*/ ePaper_Cs,
        /*DC*/ ePaper_Dc,
        /*RST*/ePaper_Rst);

    display = new GxEPD_Class(
        *io,
        /*RST*/ ePaper_Rst,
        /*BUSY*/ ePaper_Busy);

    dispPort->begin();
    display->init(/*115200*/);
    display->setRotation(3);
    display->fillScreen(GxEPD_WHITE);
    display->setTextColor(GxEPD_BLACK);
    display->setFont(&FreeMonoBold9pt7b);
    display->update();
}

void drawModeIcon(OperationMode mode) {
    if (mode == PTT) {
        //display->drawBitmap(0, 0, (const uint8_t *)ptt_icon, 16, 16, GxEPD_BLACK);
    } else if (mode == TXT) {
        //display->drawBitmap(0, 0, (const uint8_t *)txt_icon, 16, 16, GxEPD_BLACK);
    }
}

void updDisp(uint8_t line, const char* msg) {
    if (line < 10 && strcmp(disp_buf[line], msg) != 0) {  
        strncpy(disp_buf[line], msg, sizeof(disp_buf[line]) - 1);
        disp_buf[line][sizeof(disp_buf[line]) - 1] = '\0'; 

        Serial.println(msg);

        drawModeIcon(current_mode);

        for (uint8_t i = 0; i < 10; i++) {
            if (i < 2 ) {
                //Make room form the icon
                display->setCursor(20, disp_top_margin + i * disp_font_height);
                display->fillRect(20,disp_top_margin + (i * disp_font_height), disp_width, disp_window_heigth_fix,GxEPD_WHITE);
            } else {
                display->setCursor(0, disp_top_margin + i * disp_font_height);
                display->fillRect(0,disp_top_margin + (i * disp_font_height), disp_width, disp_window_heigth_fix,GxEPD_WHITE);
            }
            display->setTextColor(GxEPD_BLACK);
            display->setFont(&FreeMonoBold9pt7b);
            //First clear this line
            printline(disp_buf[i]);
            if(i==line) {
              //This is the changed line, so make sure to refresh the display
              //Seems like update window does not print the top, so let's reduce the Y by 8.
              display->updateWindow(0, disp_top_margin+(i*disp_font_height)-disp_window_offset,disp_width, disp_window_heigth_fix,true);
            }

        }

    }
}

void updateMessageDisplay() {
    //display->fillScreen(GxEPD_WHITE);
    //display->setTextColor(GxEPD_BLACK);
    //display->setFont(&FreeMonoBold9pt7b);
    //display->setFont(&FreeMonoBold12pt7b);

    for (int i = 0; i < 10; i++) {
        display->setCursor(0, 10*disp_font_height + i * disp_font_height);
        printline(message_lines[i]);
    }
    //display->update();
    display->updateWindow(0, 0, 200, 200,true);
}

void updModeAndChannelDisplay() {
    drawModeIcon(current_mode);
    char buf[30];
    snprintf(buf, sizeof(buf), "chn:%c - %d bps", channels[channel_idx], getBitrateFromIndex(bitrate_idx));
    updDisp(0,buf);
}

void showError(const char* error_msg) {
    // Display error message on the bottom line
    updDisp(3, error_msg);
    //updateMessageDisplay();
}

void enableBacklight(bool en)
{
    digitalWrite(ePaper_Backlight, en);
}
