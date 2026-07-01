#include "display_layout.h"
#include "app_modes.h"
#include "battery.h"
#include "gps.h"
#include "settings.h"
#include "ble.h"
#include "lora.h"
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/Org_01.h>

// ── LayoutState definition ──
LayoutState layout_state;

void initLayoutState() {
    memset(&layout_state, 0, sizeof(layout_state));
    layout_state.mode = current_mode;
    layout_state.beacon_distance = 0;
    layout_state.range_distance_m = 0;
    layout_state.range_sender = false;
    layout_state.ptt_tx_active = false;
    layout_state.ptt_rx_active = false;
    layout_state.ptt_sending = false;
    layout_state.ptt_receiving = false;
    layout_state.ptt_state_changed_ms = 0;
    layout_state.pong_state = 0;
    layout_state.pong_rtt_ms = 0;
    layout_state.scan_progress_pct = 0;
    memset(layout_state.scan_current_freq, 0, sizeof(layout_state.scan_current_freq));
    memset(layout_state.raw_hex_line1, 0, sizeof(layout_state.raw_hex_line1));
    memset(layout_state.raw_ascii_line, 0, sizeof(layout_state.raw_ascii_line));
    layout_state.tst_sent = 0;
    layout_state.tst_rcvd = 0;
    layout_state.wp_lat = 0;
    layout_state.wp_lon = 0;
    layout_state.wp_alt = 0;
    memset(layout_state.wp_label, 0, sizeof(layout_state.wp_label));
    layout_state.wp_broadcasting = false;
    layout_state.wp_bcast_remaining_s = 0;
}

// ── Primitive: draw header row (y=12) ──
void drawHeaderRow(const char* mode_name, const char* channel_sf) {
    drawModeIcon(mode_name);

    int name_x = 24;
    display->setFont(&FreeMonoBold9pt7b);

    // Black box behind mode name — text centered vertically in the 16px bar
    display->fillRect(name_x, disp_top_margin, 100, 16, GxEPD_BLACK);
    display->setCursor(name_x + 4, disp_top_margin + 11);
    display->setTextColor(GxEPD_WHITE);
    display->print(mode_name);

    // Channel/SF right-aligned at far-right edge
    int channel_x = disp_width - 90;
    display->fillRect(channel_x, disp_top_margin, 86, 16, GxEPD_BLACK);
    display->setCursor(channel_x + 4, disp_top_margin + 11);
    display->setTextColor(GxEPD_WHITE);
    display->print(channel_sf);

    display->setTextColor(GxEPD_BLACK);
}

// ── Primitive: black box tag ──
void drawBlackBoxTag(const char* text, int x, int y, int w, int h) {
    display->fillRect(x, y, w, h, GxEPD_BLACK);
    // Estimate text width to center in box
    int text_w = strlen(text) * 7;  // approximate for FreeMonoBold9pt7b
    int tx = x + (w - text_w) / 2;
    display->setCursor(tx, y + h - 5);
    display->setFont(&FreeMonoBold9pt7b);
    display->setTextColor(GxEPD_WHITE);
    display->print(text);
    display->setTextColor(GxEPD_BLACK);
}

// ── Primitive: solid divider bar ──
void drawSolidBar(int x, int y, int w, int h, uint16_t color) {
    // Minimum 3px for visual weight; if h < 3, fill up to 3
    int draw_h = (h < 3) ? 3 : h;
    display->fillRect(x, y, w, draw_h, GxEPD_BLACK);
}

// ── Primitive: bordered rectangle ──
void drawBorderedRect(int x, int y, int w, int h) {
    display->fillRect(x, y, w, h, GxEPD_WHITE);
    display->drawRect(x, y, w, h, GxEPD_BLACK);
}

// ── Primitive: primary value text (centered horizontally at given Y) ──
void drawPrimaryValue(const char* text, const GFXfont* font, int x, int y) {
    display->setFont(font);
    int text_w = strlen(text) * 8;  // approximate
    int cx = (disp_width - text_w) / 2;
    display->setCursor(cx, y + 10);
    display->setTextColor(GxEPD_BLACK);
    display->print(text);
}

// ── Primitive: secondary row (left-aligned at given Y) ──
void drawSecondaryRow(const char* text, int y) {
    display->setFont(&FreeMonoBold9pt7b);
    display->setCursor(12, y + 10);
    display->setTextColor(GxEPD_BLACK);
    display->print(text);
}

// ── Primitive: monospace aligned text ──
void drawMonospaceAligned(const char* text, int x, int y) {
    display->setFont(&FreeMonoBold9pt7b);
    display->setCursor(x, y + 10);
    display->setTextColor(GxEPD_BLACK);
    display->print(text);
}

// ── Primitive: bottom status bar (y=168) ──
void drawBottomStatusbar() {
    // Fill entire 200x32 rect with BLACK first
    display->fillRect(0, disp_height - 32, disp_width, 32, GxEPD_BLACK);

    int sb_y = disp_height - 32;

    // Frequency (white text, x=4)
    char freq_str[12];
    snprintf(freq_str, sizeof(freq_str), "%.2f", currentFrequency);
    display->setFont(&FreeMonoBold9pt7b);
    display->setCursor(4, sb_y + 20);
    display->setTextColor(GxEPD_WHITE);
    display->print(freq_str);

    // Time (white text, center ~95)
    RTC_Date dateTime = rtc.getDateTime();
    char time_str[9];
    snprintf(time_str, sizeof(time_str), "%02d:%02d", dateTime.hour, dateTime.minute);
    int time_w = strlen(time_str) * 8;
    display->setCursor((disp_width - time_w) / 2, sb_y + 20);
    display->print(time_str);

    // Sat count (white text, ~155)
    display->setCursor(152, sb_y + 20);
    display->print(gps_satellites);

    // Battery icon (white, far-right) — use bat icons via drawIcon
    uint8_t batt_pct = getBatteryPercentage();
    const uint16_t* batt_icon = nullptr;
    if (batt_pct > 90) batt_icon = bat100_icon;
    else if (batt_pct > 80) batt_icon = bat80_icon;
    else if (batt_pct > 60) batt_icon = bat60_icon;
    else if (batt_pct > 40) batt_icon = bat40_icon;
    else if (batt_pct > 20) batt_icon = bat20_icon;
    else if (batt_pct > 10) batt_icon = bat10_icon;
    else batt_icon = bat0_icon;

    drawIcon(batt_icon, disp_width - disp_icon_width - disp_right_margin, sb_y + 2, disp_icon_height, disp_icon_width, GxEPD_BLACK, GxEPD_WHITE);

    // Revert colors
    display->setTextColor(GxEPD_BLACK);
}

// ── Default layout: header row + bottom status bar ──
void drawDefaultLayout() {
    display->setFullWindow();
    display->firstPage();
    do {
        // Fill entire screen with white only once within the page loop.
        // GxEPD2 firstPage()/nextPage() internally handles the partial/full refresh
        // cycle, so we must NOT call display->refresh() ourselves here.
        display->fillScreen(GxEPD_WHITE);

        char chan_sf[20];
        if (current_mode == "PTT") {
            snprintf(chan_sf, sizeof(chan_sf), "chn:%c %dbps", channels[deviceSettings.channel_idx], getBitrateFromIndex(deviceSettings.bitrate_idx));
        } else {
            snprintf(chan_sf, sizeof(chan_sf), "chn:%c spf:%d", channels[deviceSettings.channel_idx], deviceSettings.spreading_factor);
        }

        drawHeaderRow(current_mode, chan_sf);
        drawBottomStatusbar();

        display->fillRect(0, 32, disp_width, disp_height - 64, GxEPD_WHITE);
    } while (display->nextPage());
}

// ── Per-mode: BEACON (placeholder) ──
void drawBeaconLayout() {
    // Clear white
    display->fillScreen(GxEPD_WHITE);

    char chan_sf[20];
    snprintf(chan_sf, sizeof(chan_sf), "chn:%c spf:%d", channels[deviceSettings.channel_idx], deviceSettings.spreading_factor);
    drawHeaderRow("BEACON", chan_sf);

    // Peer name and distance placeholder area
    display->fillRect(12, 50, disp_width - 24, 80, GxEPD_WHITE);
    drawSecondaryRow("Peer: ---", 50);
    drawSecondaryRow("Dist: --- m", 75);

    drawBottomStatusbar();
}

// ── Per-mode: RANGE (placeholder) ──
void drawRangeLayout() {
    display->fillScreen(GxEPD_WHITE);

    char chan_sf[20];
    snprintf(chan_sf, sizeof(chan_sf), "chn:%c spf:%d", channels[deviceSettings.channel_idx], deviceSettings.spreading_factor);
    drawHeaderRow("RANGE", chan_sf);

    display->fillRect(12, 50, disp_width - 24, 80, GxEPD_WHITE);
    drawSecondaryRow("Distance: --- m", 60);
    drawSecondaryRow("Role: ---", 85);

    drawBottomStatusbar();
}

// ── Per-mode: PTT Big Card TX/RX state block ──
void drawPttLayout() {
    // ── Determine PTT state ──
    char state_text[24];
    uint16_t bg_color, text_color;
    
    if (layout_state.ptt_tx_active) {
        snprintf(state_text, sizeof(state_text), "SENDING");
        bg_color = GxEPD_BLACK;
        text_color = GxEPD_WHITE;
    } else if (layout_state.ptt_rx_active) {
        snprintf(state_text, sizeof(state_text), "RECEIVING");
        bg_color = GxEPD_WHITE;
        text_color = GxEPD_BLACK;
    } else {
        snprintf(state_text, sizeof(state_text), "STANDBY  ");
        bg_color = GxEPD_WHITE;
        text_color = GxEPD_BLACK;
    }

    // ── State block at y=48 — bordered rect (176x82 pixels) ──
    int blk_x = 12, blk_y = 48, blk_w = 176, blk_h = 82;

    // Fill background first (critical for e-ink inversion safety)
    display->fillRect(blk_x, blk_y, blk_w, blk_h, bg_color);

    // Draw border on top of filled area
    display->drawRect(blk_x, blk_y, blk_w, blk_h, GxEPD_BLACK);

    // ── State text centered in block ──
    int title_line = blk_y + 22;     // first text row (state label)
    int activity_line = blk_y + 48;  // second text row (activity dots)

    display->setFont(&FreeMonoBold12pt7b);
    display->setCursor(blk_x + 6, title_line + 10);
    display->setTextColor(text_color);
    display->print(state_text);

    // Activity indicator dots below state text (3 circles as ●●●)
    if (layout_state.ptt_tx_active) {
        const uint16_t dot_y = blk_y + 58;
        for (int i = 0; i < 3; i++) {
            int dx = blk_x + 24 + (i * 22);
            display->fillCircle(dx, dot_y, 6, text_color);
        }
    } else if (layout_state.ptt_rx_active) {
        // Audio level bars for receiving state
        const uint16_t bar_base_y = blk_y + 58;
        int bar_w = 4;
        int bar_x = blk_x + 12;
        uint16_t bar_color = (bg_color == GxEPD_WHITE) ? GxEPD_BLACK : GxEPD_WHITE;
        
        const int heights[] = {8, 14, 10, 16, 12, 6, 14, 10};
        for (int i = 0; i < 8; i++) {
            display->fillRect(bar_x + (i * (bar_w + 3)), bar_base_y - heights[i], bar_w, heights[i], bar_color);
        }
    }

    // ── Channel / bitrate info below state block ──
    display->setFont(&FreeMonoBold9pt7b);
    display->setCursor(12, blk_y + blk_h + 20);
    display->setTextColor(GxEPD_BLACK);
    
    char chan_detail[32];
    snprintf(chan_detail, sizeof(chan_detail), "chn:%c %dbps", 
             channels[deviceSettings.channel_idx], 
             getBitrateFromIndex(deviceSettings.bitrate_idx));
    display->print(chan_detail);

    // ── Signal metrics on next row ──
    display->setCursor(12, blk_y + blk_h + 36);
    
    char sig_buf[50];
    float rssi_val = (radio != nullptr) ? radio->getRSSI() : -127.0f;
    float snr_val = (radio != nullptr) ? radio->getSNR() : 0.0f;
    snprintf(sig_buf, sizeof(sig_buf), "SNR:%.1fdB RSSI:%.0fdBm", snr_val, rssi_val);
    display->print(sig_buf);

    // ── Draw bottom status bar ──
    drawBottomStatusbar();
}

// ── Per-mode: TXT single message (placeholder) ──
void drawTxtSingleLayout() {
    display->fillScreen(GxEPD_WHITE);

    char chan_sf[20];
    snprintf(chan_sf, sizeof(chan_sf), "chn:%c spf:%d", channels[deviceSettings.channel_idx], deviceSettings.spreading_factor);
    drawHeaderRow("TXT", chan_sf);

    display->fillRect(12, 50, disp_width - 24, 80, GxEPD_WHITE);
    drawSecondaryRow("Message:", 60);

    drawBottomStatusbar();
}

// ── Per-mode: TXT inbox (placeholder) ──
void drawTxtInboxLayout() {
    display->fillScreen(GxEPD_WHITE);

    char chan_sf[20];
    snprintf(chan_sf, sizeof(chan_sf), "chn:%c spf:%d", channels[deviceSettings.channel_idx], deviceSettings.spreading_factor);
    drawHeaderRow("TXT", chan_sf);

    display->fillRect(12, 50, disp_width - 24, 80, GxEPD_WHITE);
    drawSecondaryRow("Inbox:", 60);

    drawBottomStatusbar();
}

// ── Per-mode: TST (placeholder) ──
void drawTstLayout() {
    display->fillScreen(GxEPD_WHITE);

    char chan_sf[20];
    snprintf(chan_sf, sizeof(chan_sf), "chn:%c spf:%d", channels[deviceSettings.channel_idx], deviceSettings.spreading_factor);
    drawHeaderRow("TST", chan_sf);

    display->fillRect(12, 50, disp_width - 24, 80, GxEPD_WHITE);
    drawSecondaryRow("Sent: ---", 60);
    drawSecondaryRow("Recv: ---", 85);

    drawBottomStatusbar();
}

// ── Per-mode: PONG (placeholder) ──
void drawPongLayout() {
    display->fillScreen(GxEPD_WHITE);

    char chan_sf[20];
    snprintf(chan_sf, sizeof(chan_sf), "chn:%c spf:%d", channels[deviceSettings.channel_idx], deviceSettings.spreading_factor);
    drawHeaderRow("PONG", chan_sf);

    display->fillRect(12, 50, disp_width - 24, 80, GxEPD_WHITE);
    drawSecondaryRow("State: ---", 60);
    drawSecondaryRow("RTT: --- ms", 85);

    drawBottomStatusbar();
}

// ── Per-mode: SCAN (placeholder) ──
void drawScanLayout() {
    display->fillScreen(GxEPD_WHITE);

    char chan_sf[20];
    snprintf(chan_sf, sizeof(chan_sf), "chn:%c spf:%d", channels[deviceSettings.channel_idx], deviceSettings.spreading_factor);
    drawHeaderRow("SCAN", chan_sf);

    display->fillRect(12, 50, disp_width - 24, 80, GxEPD_WHITE);
    drawSecondaryRow("Freq: --- MHz", 60);
    drawSecondaryRow("Progress: ---%", 85);

    drawBottomStatusbar();
}

// ── Per-mode: RAW (placeholder) ──
void drawRawLayout() {
    display->fillScreen(GxEPD_WHITE);

    char chan_sf[20];
    snprintf(chan_sf, sizeof(chan_sf), "chn:%c spf:%d", channels[deviceSettings.channel_idx], deviceSettings.spreading_factor);
    drawHeaderRow("RAW", chan_sf);

    display->fillRect(12, 50, disp_width - 24, 80, GxEPD_WHITE);
    drawSecondaryRow("---", 60);
    drawSecondaryRow("---", 85);

    drawBottomStatusbar();
}

// ── Per-mode: WP (placeholder) ──
void drawWpLayout() {
    display->fillScreen(GxEPD_WHITE);

    char chan_sf[20];
    snprintf(chan_sf, sizeof(chan_sf), "chn:%c spf:%d", channels[deviceSettings.channel_idx], deviceSettings.spreading_factor);
    drawHeaderRow("WP", chan_sf);

    display->fillRect(12, 50, disp_width - 24, 80, GxEPD_WHITE);
    drawSecondaryRow("Lat: ---", 60);
    drawSecondaryRow("Lon: ---", 85);

    drawBottomStatusbar();
}
