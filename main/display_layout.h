#ifndef DISPLAY_LAYOUT_H
#define DISPLAY_LAYOUT_H

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include "display.h"

// ── Primitives (used by every mode) ──
void drawHeaderRow(const char* mode_name, const char* channel_sf);
void drawBottomStatusbar();
void drawBlackBoxTag(const char* text, int x, int y, int w, int h);
void drawSolidBar(int x, int y, int w, int h, uint16_t color);
void drawBorderedRect(int x, int y, int w, int h);
void drawPrimaryValue(const char* text, const GFXfont* font, int x, int y);
void drawSecondaryRow(const char* text, int y);
void drawMonospaceAligned(const char* text, int x, int y);

// ── Per-mode layouts (called from app_modes) ──
void drawBeaconLayout();
void drawRangeLayout();
void drawPttLayout();
void drawTxtSingleLayout();
void drawTxtInboxLayout();
void drawTstLayout();
void drawPongLayout();
void drawScanLayout();
void drawRawLayout();
void drawWpLayout();

// ── Layout state (current mode's data) ──
struct LayoutState {
    const char* mode;
    int beacon_peer_name;     // index into display buffer for beacon peer name
    double beacon_distance;
    int range_distance_m;
    bool range_sender;
    bool ptt_tx_active;       // true during transmission (audio being relayed to LoRa)
    bool ptt_rx_active;       // true during reception (audio being forwarded to BLE)
    bool ptt_sending;         // legacy alias — kept for backward compat
    bool ptt_receiving;       // legacy alias — kept for backward compat
    unsigned long ptt_state_changed_ms;  // timestamp of last state transition
    int pong_state;           // 0=idle, 1=sending, 2=receive
    int pong_rtt_ms;
    int scan_progress_pct;
    char scan_current_freq[16];
    char raw_hex_line1[40];
    char raw_ascii_line[40];
    int tst_sent;
    int tst_rcvd;
    double wp_lat, wp_lon;
    float wp_alt;
    char wp_label[25];
    bool wp_broadcasting;
    uint32_t wp_bcast_remaining_s;
};
extern LayoutState layout_state;
void initLayoutState();

// ── Default layout (header + statusbar) for modes that haven't implemented a custom layout yet ──
void drawDefaultLayout();

#endif
