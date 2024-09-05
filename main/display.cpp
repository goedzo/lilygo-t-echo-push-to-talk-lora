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
SPIClass        *rfPort    = nullptr;
GxIO_Class      *io        = nullptr;
GxEPD_Class     *display   = nullptr;


// Display buffer for each line (Top line, middle, bottom, and errors)
char disp_buf[4][24] = {
    "chn:A Bitrate: 1300 bps",  // Top line with channel and bitrate
    "Idle...",                 // Middle line
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

        for (uint8_t i = 0; i < 4; i++) {
            if (i == 0) {
                display->setCursor(20, 16);
            } else {
                display->setCursor(0, 32 + i * 16);
            }
            display->setTextColor(GxEPD_BLACK);
            display->setFont(&FreeMonoBold9pt7b);
            printline(disp_buf[i]);
        }

    }
}

void updateMessageDisplay() {
    //display->fillScreen(GxEPD_WHITE);
    //display->setTextColor(GxEPD_BLACK);
    //display->setFont(&FreeMonoBold9pt7b);
    //display->setFont(&FreeMonoBold12pt7b);

    for (int i = 0; i < 10; i++) {
        display->setCursor(0, 40 + i * 16);
        printline(message_lines[i]);
    }
    //display->update();
    display->updateWindow(0, 0, 200, 200,true);
}

void updModeAndChannelDisplay() {
    drawModeIcon(current_mode);

    display->setCursor(20, 16);
    display->setTextColor(GxEPD_BLACK);
    display->setFont(&FreeMonoBold9pt7b);

    char buf[30];
    snprintf(buf, sizeof(buf), "chn:%c - %d bps", channels[channel_idx], getBitrateFromIndex(bitrate_idx));
    printline(buf);

    for (uint8_t i = 1; i < 4; i++) {
        display->setCursor(0, 32 + i * 40);
        printline(disp_buf[i]);
    }
    updateMessageDisplay();
}

void showError(const char* error_msg) {
    // Display error message on the bottom line
    updDisp(3, error_msg);
    updateMessageDisplay();
}

void enableBacklight(bool en)
{
    digitalWrite(ePaper_Backlight, en);
}
