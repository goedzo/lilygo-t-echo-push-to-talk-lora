#include "utilities.h"
#include <stdint.h>
#include "settings.h"
#include "app_modes.h"
#include "battery.h"
#include "gps.h"
#include "lora.h"
#include "ble.h"

#include <GxIO/GxIO_SPI/GxIO_SPI.h>
#include <GxIO/GxIO.h>
#include <GxDEPG0150BN/GxDEPG0150BN.h>
#include "epd/GxEPD2_150_BN.h"
#include <GxEPD2_BW.h>

#include <Fonts/Org_01.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include "display.h"

int disp_top_margin = 12;
int disp_bottom_margin = 3;
int disp_right_margin = 2;
int disp_font_height = 16;
int disp_icon_height = 16;
int disp_icon_width = 16;
int disp_window_offset = 11;
int disp_height = 200;
int disp_width = 200;

// ── Icon bitmap data (unchanged) ──
const uint16_t txt_icon[16] = {
    0b1111111111111111, 0b1000000000000001, 0b1011111111111101, 0b1000111111110001,
    0b1000001111000001, 0b1000000000000001, 0b1000000000000001, 0b1000000000000001,
    0b1000000000000001, 0b1000000000000001, 0b1000000000000001, 0b1000000000000001,
    0b1000000000000001, 0b1000000000000001, 0b1111111111111111, 0b0000000000000000
};

const uint16_t ptt_icon[16] = {
    0b0000001111000000, 0b0000011111100000, 0b0000111111110000, 0b0000110000110000,
    0b0000110000110000, 0b0000110000110000, 0b0011111111111100, 0b0111111111111110,
    0b0111111111111110, 0b0111111111111110, 0b0111111111111110, 0b0111111111111110,
    0b0111111111111110, 0b0111111111111110, 0b0011111111111100, 0b0000000000000000
};

const uint16_t settings_icon[16] = {
    0b0000000000000000, 0b0000000000000001, 0b0000000000000011, 0b0000000000000111,
    0b0000000000001110, 0b0000000000011100, 0b0000000000111000, 0b0000000001110000,
    0b1000000011100000, 0b1100000111000000, 0b1110001110000000, 0b0111011100000000,
    0b0011111000000000, 0b0001110000000000, 0b0000100000000000, 0b0000000000000000
};

const uint16_t test_icon[16] = {
    0b1111111111111111, 0b1000000000000001, 0b1011111111111101, 0b1010000000000101,
    0b1010111111110101, 0b1010100000000101, 0b1010101111110101, 0b1010101000000101,
    0b1010101011111101, 0b1010101010000101, 0b1010101010110101, 0b1010101010100101,
    0b1010101010101101, 0b1010101010101001, 0b1010101010101011, 0b1111111111111111
};

const uint16_t raw_icon[16] = {
    0b0000001111000000, 0b0000111111110000, 0b0001111111111000, 0b0011110000111100,
    0b0011100000011100, 0b0011100000011100, 0b0011110000111100, 0b0001111111111000,
    0b0000111111110000, 0b0000001111000000, 0b0000000001000000, 0b0000000011000000,
    0b0000000110000000, 0b0000000100000000, 0b0000000000000000, 0b0000000000000000
};

const uint16_t range_icon[16] = {
    0b0000001111000000, 0b0000110000110000, 0b0001000000001000, 0b0010000000000100,
    0b0100000000000010, 0b0100000001100010, 0b1000000111100001, 0b1000111111000001,
    0b1000011111000001, 0b1000000110000001, 0b0100000110000010, 0b0100000010000010,
    0b0010000000000100, 0b0001000000001000, 0b0000110000110000, 0b0000001111000000
};

const uint16_t pong_icon[16] = {
    0b0000001111100000, 0b0000011111110000, 0b0000111110011000, 0b0001111110001100,
    0b0001111111001100, 0b0001111111111100, 0b0000111111111000, 0b0000111111111000,
    0b0000010101010000, 0b0000001000100000, 0b0000000101000000, 0b0000000101000000,
    0b0000000101000000, 0b0000000101000000, 0b0000000101000000, 0b0000000111000000
};

const uint16_t beacon_icon[16] = {
    0b0000000000000000, 0b0000011001100000, 0b0000111111110000, 0b0001100000011000,
    0b0001100110011000, 0b0001100000011000, 0b0001100110011000, 0b0001111111111000,
    0b0001100110011000, 0b0001100000011000, 0b0001100110011000, 0b0001100000011000,
    0b0000111111110000, 0b0000011001100000, 0b0000000000000000, 0b0000000000000000
};

const uint16_t black_icon[16] = {
    0b1111111111111111, 0b1111111111111111, 0b1111111111111111, 0b1111111111111111,
    0b1111111111111111, 0b1111111111111111, 0b1111111111111111, 0b1111111111111111,
    0b1111111111111111, 0b1111111111111111, 0b1111111111111111, 0b1111111111111111,
    0b1111111111111111, 0b1111111111111111, 0b1111111111111111, 0b1111111111111111
};

const uint16_t bat100_icon[16] = {
    0b0000000000000000, 0b0000000000000000, 0b0000000000000000, 0b0111111111111110,
    0b0111111111111110, 0b1000000000000011, 0b1010101010101011, 0b1010101010101011,
    0b1010101010101011, 0b1000000000000011, 0b0111111111111110, 0b0111111111111110,
    0b0000000000000000, 0b0000000000000000, 0b0000000000000000, 0b0000000000000000
};

const uint16_t bat80_icon[16] = {
    0b0000000000000000, 0b0000000000000000, 0b0000000000000000, 0b0111111111111110,
    0b0111111111111110, 0b1000000000000011, 0b1000101010101011, 0b1000101010101011,
    0b1000101010101011, 0b1000000000000011, 0b0111111111111110, 0b0111111111111110,
    0b0000000000000000, 0b0000000000000000, 0b0000000000000000, 0b0000000000000000
};

const uint16_t bat60_icon[16] = {
    0b0000000000000000, 0b0000000000000000, 0b0000000000000000, 0b0111111111111110,
    0b0111111111111110, 0b1000000000000011, 0b1000001010101011, 0b1000001010101011,
    0b1000001010101011, 0b1000000000000011, 0b0111111111111110, 0b0111111111111110,
    0b0000000000000000, 0b0000000000000000, 0b0000000000000000, 0b0000000000000000
};

const uint16_t bat40_icon[16] = {
    0b0000000000000000, 0b0000000000000000, 0b0000000000000000, 0b0111111111111110,
    0b0111111111111110, 0b1000000000000011, 0b1000000010101011, 0b1000000010101011,
    0b1000000010101011, 0b1000000000000011, 0b0111111111111110, 0b0111111111111110,
    0b0000000000000000, 0b0000000000000000, 0b0000000000000000, 0b0000000000000000
};

const uint16_t bat20_icon[16] = {
    0b0000000000000000, 0b0000000000000000, 0b0000000000000000, 0b0111111111111110,
    0b0111111111111110, 0b1000000000000011, 0b1000000000101011, 0b1000000000101011,
    0b1000000000101011, 0b1000000000000011, 0b0111111111111110, 0b0111111111111110,
    0b0000000000000000, 0b0000000000000000, 0b0000000000000000, 0b0000000000000000
};

const uint16_t bat10_icon[16] = {
    0b0000000000000000, 0b0000000000000000, 0b0000000000000000, 0b0111111111111110,
    0b0111111111111110, 0b1000000000000011, 0b1000000000001011, 0b1000000000001011,
    0b1000000000001011, 0b1000000000000011, 0b0111111111111110, 0b0111111111111110,
    0b0000000000000000, 0b0000000000000000, 0b0000000000000000, 0b0000000000000000
};

const uint16_t bat0_icon[16] = {
    0b0000000000000000, 0b0000000000000000, 0b0000000000000000, 0b0111111111111110,
    0b0111111111111110, 0b1000000000000011, 0b1000000000000011, 0b1000000000000011,
    0b1000000000000011, 0b1000000000000011, 0b0111111111111110, 0b0111111111111110,
    0b0000000000000000, 0b0000000000000000, 0b0000000000000000, 0b0000000000000000
};

const uint16_t off_icon[16] = {
    0b0000000110000000, 0b0000000110000000, 0b0001000110001000, 0b0011000110001100,
    0b0110000110000110, 0b1100000110000011, 0b1100000110000011, 0b1100000000000011,
    0b1100000000000011, 0b1100000000000011, 0b1100000000000011, 0b0110000000000110,
    0b0011000000001100, 0b0001100000011000, 0b0000011111100000, 0b0000000000000000
};

const uint16_t gpsok_icon[16] = {
    0b0000000000000000, 0b0000000011111000, 0b0000000000001100, 0b0011000111000110,
    0b0010100001110010, 0b0010010000011011, 0b0010001110001001, 0b0010000101001000,
    0b0010000011000000, 0b0010000001000000, 0b0001000000100000, 0b0001100000010000,
    0b0001110000001000, 0b0011001111111000, 0b0010000010000000, 0b0111111110000000
};

const uint16_t gpsnofix_icon[16] = {
    0b0000000001100011, 0b0000000000110110, 0b0000000000011100, 0b0011000000011100,
    0b0000100000110110, 0b0010010001100011, 0b0000001010000000, 0b0010000101000000,
    0b0000000010000000, 0b0010000001000000, 0b0001000000100000, 0b0000000000010000,
    0b0001010000001000, 0b0010001101010000, 0b0010000010000000, 0b0101010110000000
};

SPIClass        *dispPort  = nullptr;
GxIO_Class      *io        = nullptr;
GxEPD_Class     *display_v1   = nullptr;
GxEPD2_BW<GxEPD2_150_BN, GxEPD2_150_BN::HEIGHT>* display = nullptr;

void printline(const char* msg) {
    display->print(msg);
}

void setupDisplay() {
    SerialMon.println("[Display] starting e-paper init...");

    pinMode(ePaper_Backlight, OUTPUT);
    digitalWrite(ePaper_Backlight, HIGH);
    enableBacklight(true);
    SerialMon.println("[Display] backlight ON");

    dispPort = new SPIClass(
        NRF_SPIM2, ePaper_Miso, ePaper_Sclk, ePaper_Mosi);

    SPISettings spiSettings(4000000, MSBFIRST, SPI_MODE0);

    GxEPD2_150_BN epd(GxEPD2_150_BN(ePaper_Cs, ePaper_Dc, ePaper_Rst, ePaper_Busy));
    display = new GxEPD2_BW<GxEPD2_150_BN, GxEPD2_150_BN::HEIGHT>(epd);

    SerialMon.println("[Display] calling init with reset_duration=300...");
    display->init(0, true, 300, false, *dispPort, spiSettings);
    SerialMon.println("[Display] init() returned OK");

    delay(200);
    SerialMon.println("[Display] post-reset delay done");

    display->setRotation(3);
    SerialMon.println("[Display] rotation set, clearing screen...");

    display->clearScreen();
    SerialMon.println("[Display] clearScreen returned OK");

    display->setFullWindow();
    display->fillScreen(GxEPD_WHITE);
    display->setTextColor(GxEPD_BLACK);
    display->setFont(&FreeMonoBold9pt7b);

    display->firstPage();
    do {
        display->fillScreen(GxEPD_WHITE);
        display->setCursor(10, 40);
        display->print("T-Echo Ready!");
    } while (display->nextPage());

    SerialMon.println("[Display] doing full refresh...");
    display->refresh(false);
    SerialMon.println("[Display] COMPLETE - e-paper should be updating now");
}

void swapIconBytes(const uint16_t* originalIcon, uint16_t* swappedIcon, int size) {
    for (int i = 0; i < size; i++) {
        uint16_t val = originalIcon[i];
        uint8_t highByte = (val >> 8) & 0xFF;
        uint8_t lowByte = val & 0xFF;
        swappedIcon[i] = (lowByte << 8) | highByte;
    }
}

void drawIcon(const uint16_t* icon_data, int x, int y, int height, int width, uint16_t bg_color, uint16_t icon_color) {
    display->fillRect(x, y, height, width, bg_color);
    uint16_t swappedIcon[height];
    swapIconBytes(icon_data, swappedIcon, width);
    display->drawBitmap(x, y, (const uint8_t *)swappedIcon, height, width, icon_color);
}

void drawModeIcon(const char* mode) {
    bool icon_drawn = false;

    if (mode == "OFF") {
        drawIcon(off_icon, 0, disp_top_margin, disp_icon_height, disp_icon_width, GxEPD_WHITE, GxEPD_BLACK);
        return;
    }

    if (in_settings_mode) {
        drawIcon(settings_icon, 0, disp_top_margin, disp_icon_height, disp_icon_width, GxEPD_WHITE, GxEPD_BLACK);
        icon_drawn = true;
    } else {
        if (mode == "PTT") {
            drawIcon(ptt_icon, 0, disp_top_margin, disp_icon_height, disp_icon_width, GxEPD_WHITE, GxEPD_BLACK);
            icon_drawn = true;
        } else if (mode == "TXT") {
            drawIcon(txt_icon, 0, disp_top_margin, disp_icon_height, disp_icon_width, GxEPD_WHITE, GxEPD_BLACK);
            icon_drawn = true;
        } else if (mode == "RAW") {
            drawIcon(raw_icon, 0, disp_top_margin, disp_icon_height, disp_icon_width, GxEPD_WHITE, GxEPD_BLACK);
            icon_drawn = true;
        } else if (mode == "TST") {
            drawIcon(test_icon, 0, disp_top_margin, disp_icon_height, disp_icon_width, GxEPD_WHITE, GxEPD_BLACK);
            icon_drawn = true;
        } else if (mode == "RANGE") {
            drawIcon(range_icon, 0, disp_top_margin, disp_icon_height, disp_icon_width, GxEPD_WHITE, GxEPD_BLACK);
            icon_drawn = true;
        } else if (mode == "PONG") {
            drawIcon(pong_icon, 0, disp_top_margin, disp_icon_height, disp_icon_width, GxEPD_WHITE, GxEPD_BLACK);
            icon_drawn = true;
        } else if (mode == "BEACON") {
            drawIcon(beacon_icon, 0, disp_top_margin, disp_icon_height, disp_icon_width, GxEPD_WHITE, GxEPD_BLACK);
            icon_drawn = true;
        }
    }

    if (!icon_drawn) {
        drawIcon(black_icon, 0, disp_top_margin, disp_icon_height, disp_icon_width, GxEPD_WHITE, GxEPD_BLACK);
    }
}

void showError(const char* error_msg) {
    SerialMon.print("[Display] Error: ");
    SerialMon.println(error_msg);
}

void clearScreen() {
    display->fillScreen(GxEPD_WHITE);
    display->refresh(false);
    showError("");
}

void enableBacklight(bool en) {
    digitalWrite(ePaper_Backlight, en);
}

// ── Stub implementations — rendering moved to display_layout ──
void updDisp(uint8_t line, const char* msg, bool updateScreen) {}
void updModeAndChannelDisplay() {}
void printStatusIcons() {}
void printGPSIcon() {}
void printFrequencyIcon(bool updateScreen) {}
void printTimeIcon(bool updateScreen) {}
void printStatusOnApp() {}
void sleepDisplay() {}
