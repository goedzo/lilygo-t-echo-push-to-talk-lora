#include "utilities.h"
#include <stdint.h>
#include "settings.h"
#include "app_modes.h"
#include <GxIO/GxIO_SPI/GxIO_SPI.h>
#include <GxIO/GxIO.h>
#include <GxDEPG0150BN/GxDEPG0150BN.h>  // 1.54" b/w 
#include "epd/GxEPD2_150_BN.h"
#include <GxEPD2_BW.h>  // For black and white displays

#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include "display.h"

// Define the 16x16 pixel icon for "TXT" mode
const uint16_t txt_icon[16] = {
    // Your bitmap data for TXT mode
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
    // Your bitmap data for PTT mode
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
GxEPD_Class     *display_v1   = nullptr;
GxEPD2_BW<GxEPD2_150_BN, GxEPD2_150_BN::HEIGHT>* display = nullptr;


int disp_top_margin = 12;   // The blank margin on top
int disp_font_height = 16;  // The height of a line in pixels
int disp_window_offset = 11; // The correction for display->updateWindow to print the updated line.
int disp_height = 200;      // The height of the display
int disp_width = 200;       // The width of the display

// Display buffer for each line (Top line, middle, bottom, and errors)
char disp_buf[20][24] = {
    "",  // Top line with channel and bitrate
    "",  // Middle line
    "",  // Bottom line for errors
    ""   // 4th line for non-PTT messages
};

int displayLines=sizeof(disp_buf) / sizeof(disp_buf[0]);

void printline(const char* msg) {
    //Serial.println(msg);
    //Serial.println("\n");
    display->print(msg);
}

void setupDisplay() {
    dispPort = new SPIClass(
        /*SPIPORT*/NRF_SPIM2,
        /*MISO*/ ePaper_Miso,
        /*SCLK*/ePaper_Sclk,
        /*MOSI*/ePaper_Mosi);

    dispPort->begin();
    
    // Create SPI settings with speed, data order, and mode
    SPISettings spiSettings(4000000, MSBFIRST, SPI_MODE0);  // 4 MHz speed, MSB first, SPI mode 0

    //Now let's create the display class
    display = new GxEPD2_BW<GxEPD2_150_BN, GxEPD2_150_BN::HEIGHT>(GxEPD2_150_BN(ePaper_Cs, ePaper_Dc, ePaper_Rst, ePaper_Busy));

    display->init(115200, true, 20, false, *dispPort, spiSettings);
    display->setRotation(3); // Set display rotation
    enableBacklight(true);
    display->clearScreen();
    display->setFullWindow(); // Use the full display window
    display->fillScreen(GxEPD_WHITE); // Clear the screen
    display->setTextColor(GxEPD_BLACK); // Set text color
    display->setFont(&FreeMonoBold9pt7b); // Set default font
}

void drawModeIcon(OperationMode mode) {
    if (mode == PTT) {
        display->drawBitmap(0, 0, (const uint8_t *)ptt_icon, 16, 16, GxEPD_BLACK);
    } else if (mode == TXT) {
        display->drawBitmap(0, 0, (const uint8_t *)txt_icon, 16, 16, GxEPD_BLACK);
    }
}

void updDisp(uint8_t line, const char* msg, bool updateScreen) {
    if (line < displayLines && strcmp(disp_buf[line], msg) != 0) {  
        strncpy(disp_buf[line], msg, sizeof(disp_buf[line]) - 1);
        disp_buf[line][sizeof(disp_buf[line]) - 1] = '\0'; 

        //Serial.println(msg);

        drawModeIcon(current_mode);  // Draw the mode icon

        // Clear only the part where the updated line is
        if (line < 2) {
            // Make room for the icon if it's in the first two lines
            display->fillRect(20, disp_top_margin + (line * disp_font_height)-disp_window_offset, disp_width, disp_font_height, GxEPD_WHITE);
            display->setCursor(20, disp_top_margin + line * disp_font_height);
        } else {
            // Normal position for other lines
            display->fillRect(0, disp_top_margin + (line * disp_font_height)-disp_window_offset, disp_width, disp_font_height, GxEPD_WHITE);
            display->setCursor(0, disp_top_margin + line * disp_font_height);
        }

        // Set text color and font
        display->setTextColor(GxEPD_BLACK);
        display->setFont(&FreeMonoBold9pt7b);

        // Print the new line
        printline(disp_buf[line]);
        //Mark this line for reprinting
        if(updateScreen) {
            display->displayWindow(0,0,200,200);
        }

    }
}

void clearScreen(){
    for(int i=0;i<displayLines;i++) {
      updDisp(i,"",false);
    }
    updDisp(0,"",true);
}


void updModeAndChannelDisplay() {
    drawModeIcon(current_mode);
    char buf[30];
    snprintf(buf, sizeof(buf), "chn:%c %dbps", channels[channel_idx], getBitrateFromIndex(bitrate_idx));
    updDisp(0, buf);
}

void showError(const char* error_msg) {
    // Display error message on the bottom line
    updDisp(3, error_msg);
}

void enableBacklight(bool en) {
    digitalWrite(ePaper_Backlight, en);
}
