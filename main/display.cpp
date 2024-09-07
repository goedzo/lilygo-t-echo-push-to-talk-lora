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
    0b1111111111111111,
    0b1000000000000001,
    0b1011111111111101,
    0b1000111111110001,
    0b1000001111000001,
    0b1000000000000001,
    0b1000000000000001,
    0b1000000000000001,
    0b1000000000000001,
    0b1000000000000001,
    0b1000000000000001,
    0b1000000000000001,
    0b1000000000000001,
    0b1000000000000001,
    0b1111111111111111,
    0b0000000000000000
};

// Define the 16x16 pixel icon for "PTT" mode
const uint16_t ptt_icon[16] = {
    // Your bitmap data for PTT mode
    0b0000001111000000,
    0b0000011111100000,
    0b0000111111110000,
    0b0000110000110000,
    0b0000110000110000,
    0b0000110000110000,
    0b0011111111111100,
    0b0111111111111110,
    0b0111111111111110,
    0b0111111111111110,
    0b0111111111111110,
    0b0111111111111110,
    0b0111111111111110,
    0b0111111111111110,
    0b0011111111111100,
    0b0000000000000000
};

// Icon 10: Checkmark
const uint16_t settings_icon[16] = {
    0b0000000000000000,
    0b0000000000000001,
    0b0000000000000011,
    0b0000000000000111,
    0b0000000000001110,
    0b0000000000011100,
    0b0000000000111000,
    0b0000000001110000,
    0b1000000011100000,
    0b1100000111000000,
    0b1110001110000000,
    0b0111011100000000,
    0b0011111000000000,
    0b0001110000000000,
    0b0000100000000000,
    0b0000000000000000
};

// Icon 2: Spiral
const uint16_t test_icon[16] = {
    0b1111111111111111,
    0b1000000000000001,
    0b1011111111111101,
    0b1010000000000101,
    0b1010111111110101,
    0b1010100000000101,
    0b1010101111110101,
    0b1010101000000101,
    0b1010101011111101,
    0b1010101010000101,
    0b1010101010110101,
    0b1010101010100101,
    0b1010101010101101,
    0b1010101010101001,
    0b1010101010101011,
    0b1111111111111111
};

const uint16_t raw_icon[16] = {
    0b0000001111000000,
    0b0000111111110000,
    0b0001111111111000,
    0b0011110000111100,
    0b0011100000011100,
    0b0011100000011100,
    0b0011110000111100,
    0b0001111111111000,
    0b0000111111110000,
    0b0000001111000000,
    0b0000000001000000,
    0b0000000011000000,
    0b0000000110000000,
    0b0000000100000000,
    0b0000000000000000,
    0b0000000000000000
};

// Black Icon
const uint16_t black_icon[16] = {
    0b1111111111111111,
    0b1111111111111111,
    0b1111111111111111,
    0b1111111111111111,
    0b1111111111111111,
    0b1111111111111111,
    0b1111111111111111,
    0b1111111111111111,
    0b1111111111111111,
    0b1111111111111111,
    0b1111111111111111,
    0b1111111111111111,
    0b1111111111111111,
    0b1111111111111111,
    0b1111111111111111,
    0b1111111111111111,
};



// Icon 46: Battery 100% Full (6 lines of vertical stripes)
const uint16_t bat100_icon[16] = {
    0b0000000000000000,
    0b0000000000000000,
    0b0000000000000000,
    0b0111111111111110,
    0b0111111111111110,
    0b1000000000000011,
    0b1010101010101011,
    0b1010101010101011,
    0b1010101010101011,
    0b1000000000000011,
    0b0111111111111110,
    0b0111111111111110,
    0b0000000000000000,
    0b0000000000000000,
    0b0000000000000000,
    0b0000000000000000
};

// Icon 47: Battery 80% Full (5 lines of vertical stripes)
const uint16_t bat800_icon[16] = {
    0b0000000000000000,
    0b0000000000000000,
    0b0000000000000000,
    0b0111111111111110,
    0b0111111111111110,
    0b1000000000000011,
    0b1000101010101011,
    0b1000101010101011,
    0b1000101010101011,
    0b1000000000000011,
    0b0111111111111110,
    0b0111111111111110,
    0b0000000000000000,
    0b0000000000000000,
    0b0000000000000000,
    0b0000000000000000
};

// Icon 48: Battery 60% Full (4 lines of vertical stripes)
const uint16_t bat60_icon[16] = {
    0b0000000000000000,
    0b0000000000000000,
    0b0000000000000000,
    0b0111111111111110,
    0b0111111111111110,
    0b1000000000000011,
    0b1000001010101011,
    0b1000001010101011,
    0b1000001010101011,
    0b1000000000000011,
    0b0111111111111110,
    0b0111111111111110,
    0b0000000000000000,
    0b0000000000000000,
    0b0000000000000000,
    0b0000000000000000
};

// Icon 49: Battery 40% Full (3 lines of vertical stripes)
const uint16_t bat40_icon[16] = {
    0b0000000000000000,
    0b0000000000000000,
    0b0000000000000000,
    0b0111111111111110,
    0b0111111111111110,
    0b1000000000000011,
    0b1000000010101011,
    0b1000000010101011,
    0b1000000010101011,
    0b1000000000000011,
    0b0111111111111110,
    0b0111111111111110,
    0b0000000000000000,
    0b0000000000000000,
    0b0000000000000000,
    0b0000000000000000
};

// Icon 50: Battery 20% Full (2 lines of vertical stripes)
const uint16_t bat20_icon[16] = {
    0b0000000000000000,
    0b0000000000000000,
    0b0000000000000000,
    0b0111111111111110,
    0b0111111111111110,
    0b1000000000000011,
    0b1000000000101011,
    0b1000000000101011,
    0b1000000000101011,
    0b1000000000000011,
    0b0111111111111110,
    0b0111111111111110,
    0b0000000000000000,
    0b0000000000000000,
    0b0000000000000000,
    0b0000000000000000
};


// Icon 50: Battery 10% Full (1 lines of vertical stripes)
const uint16_t bat10_icon[16] = {
    0b0000000000000000,
    0b0000000000000000,
    0b0000000000000000,
    0b0111111111111110,
    0b0111111111111110,
    0b1000000000000011,
    0b1000000000001011,
    0b1000000000001011,
    0b1000000000001011,
    0b1000000000000011,
    0b0111111111111110,
    0b0111111111111110,
    0b0000000000000000,
    0b0000000000000000,
    0b0000000000000000,
    0b0000000000000000
};

// Icon 51: Battery Empty (0 lines, just the frame)
const uint16_t bat0_icon[16] = {
    0b0000000000000000,
    0b0000000000000000,
    0b0000000000000000,
    0b0111111111111110,
    0b0111111111111110,
    0b1000000000000011,
    0b1000000000000011,
    0b1000000000000011,
    0b1000000000000011,
    0b1000000000000011,
    0b0111111111111110,
    0b0111111111111110,
    0b0000000000000000,
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

void swapIconBytes(const uint16_t* originalIcon, uint16_t* swappedIcon, int size) {
    for (int i = 0; i < size; i++) {
        uint16_t val = originalIcon[i];
        uint8_t highByte = (val >> 8) & 0xFF;  // Extract the high byte (first 8 bits)
        uint8_t lowByte = val & 0xFF;          // Extract the low byte (last 8 bits)
        
        // Swap the high and low bytes and store in the swapped icon array
        swappedIcon[i] = (lowByte << 8) | highByte;
    }
}



void drawIcon(const uint16_t* icon_data,int x, int y,int height, int width, uint16_t bg_color, uint16_t icon_color) {
    //Clear the icon first
    display->fillRect(x, y, height, width, bg_color);
    uint16_t swappedIcon[height]; // Create a temporary buffer for the swapped icon
    //This is a default icon in when no icon is set
    swapIconBytes(icon_data, swappedIcon, width);  // Swap the bytes of the black icon
    display->drawBitmap(x, y, (const uint8_t *)swappedIcon, height, width, icon_color);

}

void drawModeIcon(const char* mode) {
    //If we are in settings, override the current icon
    bool icon_drawn=false;
    if(in_settings_mode) {
        drawIcon(settings_icon,0, disp_top_margin,16, 16, GxEPD_WHITE, GxEPD_BLACK);
        icon_drawn=true;
    }
    else {
        if (mode == "PTT") {
            drawIcon(ptt_icon,0, disp_top_margin,16, 16, GxEPD_WHITE, GxEPD_BLACK);
            icon_drawn=true;
        } else if (mode == "TXT") {
            drawIcon(txt_icon,0, disp_top_margin,16, 16, GxEPD_WHITE, GxEPD_BLACK);
            icon_drawn=true;
        } else if (mode == "RAW") {
            drawIcon(raw_icon,0, disp_top_margin,16, 16, GxEPD_WHITE, GxEPD_BLACK);
            icon_drawn=true;
        } else if (mode == "TST") {
            drawIcon(test_icon,0, disp_top_margin,16, 16, GxEPD_WHITE, GxEPD_BLACK);
            icon_drawn=true;
        }

    }

    if(!icon_drawn) {
      //We are missing an icon for this mode, so make a black icon to show it
      drawIcon(black_icon,0, disp_top_margin,16, 16, GxEPD_WHITE, GxEPD_BLACK);
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
    if (!in_settings_mode) {
        char displayString[20];
        snprintf(displayString, sizeof(displayString), "Mode: %s", current_mode);
        // Display the current mode name
        updDisp(1, displayString,true);
    }
    char buf[30];
    if(current_mode=="PTT") {
        snprintf(buf, sizeof(buf), "chn:%c %dbps", channels[channel_idx], getBitrateFromIndex(bitrate_idx));
    }
    else {
        snprintf(buf, sizeof(buf), "chn:%c spf:%d", channels[channel_idx], spreading_factor);
    }

    updDisp(0, buf,true);


}

void showError(const char* error_msg) {
    // Display error message on the bottom line
    updDisp(3, error_msg);
}

void enableBacklight(bool en) {
    digitalWrite(ePaper_Backlight, en);
}
