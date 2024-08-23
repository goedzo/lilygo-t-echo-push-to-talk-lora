#include "display.h"
#include "app_modes.h"

// Define the 16x16 pixel icon for "TXT" mode
const uint8_t txt_icon[16] = {
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
const uint8_t ptt_icon[16] = {
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

GxEPD2_BW<GxEPD2_154, GxEPD2_154::HEIGHT> disp(GxEPD2_154(/*CS=*/29, /*DC=*/27, /*RST=*/30, /*BUSY=*/31));

void setupDisplay() {
    disp.init();
    disp.setRotation(1);
    disp.setTextColor(GxEPD_BLACK);
}

void drawModeIcon(OperationMode mode) {
    if (mode == PTT) {
        disp.drawBitmap(0, 0, (const uint8_t *)ptt_icon, 16, 16, GxEPD_BLACK);
    } else if (mode == TXT) {
        disp.drawBitmap(0, 0, (const uint8_t *)txt_icon, 16, 16, GxEPD_BLACK);
    }
}

void updDisp(uint8_t line, const char* msg) {
    if (line < 4 && strcmp(disp_buf[line], msg) != 0) {  
        strncpy(disp_buf[line], msg, sizeof(disp_buf[line]) - 1);
        disp_buf[line][sizeof(disp_buf[line]) - 1] = '\0'; 

        disp.setFullWindow();
        disp.firstPage();
        do {
            disp.fillScreen(GxEPD_WHITE);
            drawModeIcon(current_mode);

            for (uint8_t i = 0; i < 4; i++) {
                if (i == 0) {
                    disp.setCursor(20, 16);
                } else {
                    disp.setCursor(0, 32 + i * 16);
                }
                disp.setTextColor(GxEPD_BLACK);
                disp.setFont(&FreeMonoBold9pt7b);
                disp.print(disp_buf[i]);
            }

        } while (disp.nextPage());
    }
}

void updateMessageDisplay() {
    disp.setFullWindow();
    disp.firstPage();
    do {
        disp.fillScreen(GxEPD_WHITE);
        disp.setTextColor(GxEPD_BLACK);
        disp.setFont(&FreeMonoBold9pt7b);

        for (int i = 0; i < 10; i++) {
            disp.setCursor(0, 40 + i * 16);
            disp.print(message_lines[i]);
        }
    } while (disp.nextPage());
}

void updModeAndChannelDisplay() {
    disp.setFullWindow();
    disp.firstPage();
    do {
        disp.fillScreen(GxEPD_WHITE);
        drawModeIcon(current_mode);

        disp.setCursor(20, 16);
        disp.setTextColor(GxEPD_BLACK);
        disp.setFont(&FreeMonoBold9pt7b);

        char buf[30];
        snprintf(buf, sizeof(buf), "chn:%c Bitrate: %d bps", channels[channel_idx], codec2_bits_per_second(codec));
        disp.print(buf);

        for (uint8_t i = 1; i < 4; i++) {
            disp.setCursor(0, 32 + i * 40);
            disp.print(disp_buf[i]);
        }

    } while (disp.nextPage());
}