#include "boot_animation.h"
#include <GxEPD2_BW.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>

extern GxEPD2_BW<GxEPD2_150_BN, GxEPD2_150_BN::HEIGHT>* display;

// ============================================================
// BOOT ANIMATION — Staged splash sequence for 200x200 e-paper
// E-paper constraints: full refresh ~4s, partial ~0.8s
// Uses staged frames with text + bitmap drawImage (uint8_t*)
// ============================================================

// --- LilyGO logo: 32px wide x 14px tall, stored as uint8_t[14][4] ---
// Each row = 4 bytes (32 bits / 8)
static const uint8_t lilygo_logo[14][4] = {
    // L   I   L   Y       O   (columns: bit31..bit0 across 4 bytes)
    {0x80, 0x10, 0x01, 0x00},  // row 0  |   |   |
    {0x80, 0x10, 0x01, 0x00},  // row 1  |   |   |
    {0x80, 0x10, 0x01, 0x00},  // row 2  |   |   |
    {0x80, 0x10, 0x3F, 0x00},  // row 3  |   |   Y curve
    {0x80, 0x10, 0x21, 0x00},  // row 4  |   |   Y narrow
    {0x80, 0x10, 0x01, 0x00},  // row 5  L   I   L Y O
    {0x80, 0x10, 0x01, 0x00},  // row 6  |   |   |
    {0x80, 0x10, 0x01, 0x00},  // row 7  |   |   |
    {0x80, 0x10, 0x21, 0x00},  // row 8  |   |   O bottom
    {0x80, 0x10, 0x3F, 0x00},  // row 9  |   |   O curve
    {0x80, 0x10, 0x01, 0x00},  // row 10 |   |   |
    {0x80, 0x10, 0x01, 0x00},  // row 11
    {0x80, 0x10, 0x00, 0x00},  // row 12
    {0x80, 0x10, 0x00, 0x00},  // row 13
};

// --- Icon helpers: each is a compact uint8_t bitmap (1 byte = 8 pixels) ---

static const uint8_t checkmark_bmp[8] = {
    0x00, 0x1C, 0x36, 0x63, 0xC6, 0x36, 0x1C, 0x00
};

static const uint8_t cross_bmp[8] = {
    0x00, 0x1C, 0x36, 0x63, 0x63, 0xC6, 0x36, 0x1C
};

static const uint8_t lora_antenna[8] = {
    0x00, 0xFC, 0x60, 0xFC, 0x04, 0x04, 0x18, 0x00
};

static const uint8_t gps_icon[8] = {
    0x00, 0x7C, 0x60, 0x90, 0xA0, 0xC0, 0x7C, 0x00
};

static const uint8_t ble_bmp[8] = {
    0x00, 0x60, 0x90, 0x88, 0x90, 0x60, 0x18, 0x00
};


// ============================================================
// FRAME 1 — Black flash (500ms): clears controller state
// ============================================================
static void frame_blackFlash() {
    display->setFullWindow();
    display->firstPage();
    do {
        display->fillScreen(GxEPD_BLACK);
    } while (display->nextPage());
    display->refresh(false);  // full refresh — panel settles to black
}

// ============================================================
// FRAME 2 — Logo + "T-Echo" (full frame, ~4s e-paper)
// ============================================================
static void drawLogo() {
    int logo_x = (200 - 32) / 2;  // center: x=84
    for (int row = 0; row < 14; row++) {
        for (int col = 0; col < 32; col += 8) {
            uint8_t byte_pixels = lilygo_logo[row][col / 8];
            for (int bit = 0; bit < 8 && (col + bit) < 32; bit++) {
                if ((byte_pixels >> (7 - bit)) & 1) {
                    display->fillRect(logo_x + col + bit, row * 3 + 20, 3, 3, GxEPD_BLACK);
                }
            }
        }
    }
}

static void frame_logo() {
    int logo_x = (200 - 32) / 2;
    
    display->setFullWindow();
    display->firstPage();
    do {
        display->fillScreen(GxEPD_WHITE);
        drawLogo();
        
        // "T-Echo" subtitle
        display->setFont(&FreeMonoBold12pt7b);
        display->setCursor((200 - 60) / 2, logo_x + 56);
        display->print("T-Echo");
    } while (display->nextPage());
    display->refresh(false);  // full — first content frame must be full
}

// ============================================================
// FRAME 3 — Tag bar below logo (partial refresh)
// Background is already white; new black elements only
// ============================================================
static void frame_tag() {
    int tag_bx = (200 - 70) / 2;  // center for tag bar
    
    display->setPartialWindow(0, 80, 200, 30);
    display->firstPage();
    do {
        display->fillScreen(GxEPD_WHITE);
        display->fillRect(tag_bx, 90, 70, 24, GxEPD_BLACK);
        display->setFont(&FreeMonoBold12pt7b);
        display->setTextColor(GxEPD_WHITE, GxEPD_BLACK);
        display->setCursor(tag_bx + 18, 106);
        display->print("PTT");
    } while (display->nextPage());
    display->refresh(true);  // partial — safe because bg is already white
}

// ============================================================
// FRAME 4 — System checks: BLE / LoRa / GPS status lines
// Each line drawn sequentially with delays between
// ============================================================
static void drawCheckLines(bool lora_ok, bool gps_ok) {
    // Draw all lines on current screen context
    display->setFont(&FreeMonoBold9pt7b);
    
    // BLE (always OK — passed setupBLE)
    display->setTextColor(GxEPD_BLACK);
    display->setCursor(30, 148);
    display->print("BLE");
    display->drawImage(ble_bmp, 62, 140, 8, 8);
    display->drawImage(checkmark_bmp, 90, 140, 8, 8);
    
    // LoRa
    display->setCursor(30, 162);
    display->print("LoRa");
    display->drawImage(lora_antenna, 64, 154, 8, 8);
    if (lora_ok) {
        display->drawImage(checkmark_bmp, 90, 154, 8, 8);
    } else {
        display->drawImage(cross_bmp, 90, 154, 8, 8);
    }
    
    // GPS
    display->setCursor(30, 176);
    display->print("GPS");
    display->drawImage(gps_icon, 62, 168, 8, 8);
    if (gps_ok) {
        display->drawImage(checkmark_bmp, 88, 168, 8, 8);
    } else {
        display->drawImage(cross_bmp, 88, 168, 8, 8);
    }
}

// ============================================================
// PUBLIC API — full animation sequence
// ============================================================

void showBootAnimation(bool lora_ok, bool gps_ok) {
    // Frame 1: Black flash (400ms)
    frame_blackFlash();
    delay(300);
    
    // Frame 2: Logo + "T-Echo" (~4s e-paper update)
    frame_logo();
    delay(500);
    
    // Frame 3: Tag bar (partial, ~0.8s)
    frame_tag();
    delay(300);
    
    // Frame 4a: BLE check (immediate)
    display->setPartialWindow(0, 135, 200, 20);
    display->firstPage();
    do {
        drawCheckLines(lora_ok, gps_ok);
    } while (display->nextPage());
    delay(400);
    
    // Final full refresh for crispness
    display->refresh(false);
}

void showBootLogo() {
    // Simple fallback: logo + "T-Echo" on white
    display->setFullWindow();
    display->firstPage();
    do {
        display->fillScreen(GxEPD_WHITE);
        drawLogo();
        int logo_x = (200 - 32) / 2;
        display->setFont(&FreeMonoBold12pt7b);
        display->setCursor((200 - 60) / 2, logo_x + 56);
        display->print("T-Echo");
    } while (display->nextPage());
    display->refresh(false);
}
