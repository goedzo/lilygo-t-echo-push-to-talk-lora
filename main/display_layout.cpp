#include "display_layout.h"
#include "utilities.h"  // for ePaper_Busy pin definition
#include "app_modes.h"
#include "battery.h"
#include "gps.h"
#include "settings.h"
#include "ble.h"
#include "lora.h"
#include "text_inbox.h"
#include "scan.h"
#include "screen_sync.h"
#include "disp_refresh.h"  // For triggerEpdRefresh + epdSetGRAMWindow + epdWriteAndRefreshRect
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/Org_01.h>

// Extern for pendingFullRefresh() from display.cpp
extern bool pendingFullRefresh();

// ── LayoutState definition ──
LayoutState layout_state;

void initLayoutState() {
    memset(&layout_state, 0, sizeof(layout_state));
    layout_state.mode = current_mode;
    layout_state._dirty = false;
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
    layout_state.scan_channel_count = 0;
    memset(layout_state.scan_channels, 0, sizeof(layout_state.scan_channels));
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

// ── Internal: common page-loop for all layouts.
//     use_full_refresh=true  → write content to GRAM + full waveform (~3.8s, no flicker)
//     use_full_refresh=false → two-step GRAM write: white→refresh→black→refresh (~1.6s, no flicker)
//     Writes directly to SSD1681 GRAM (not GxEPD2 buffer model):
//       Step 1: Write WHITE bytes to the full partial region in GRAM → trigger refresh (~0.8s)
//       Step 2: Write actual content to GRAM via firstPage/nextPage → trigger refresh (~0.8s)
// ──

typedef void (*drawFn)();

static void renderPartialWithWhiteout(drawFn drawContent) {
    uint16_t x = 0, y = 12, w = disp_width, h = 184;

    // Step 1: White-out the region in GRAM → partial refresh to clear ghosting.
    epdWriteAndRefreshRect(x, y, w, h, 0xFF);

    // Step 2: Write content via GxEPD2 buffer → write to GRAM → trigger refresh.
    // First set the GRAM pointer for this region.
    display->setPartialWindow(x, y, w, h);
    epdSetGRAMWindow(x, y, w, h);

    display->firstPage();
    do {
        display->fillScreen(GxEPD_WHITE);
        drawContent();
    } while (display->nextPage());

    triggerEpdRefresh(false);
}

static void renderPageLoop(drawFn drawContent, bool use_full_refresh) {
    if (use_full_refresh) {
        // Full refresh: write content to GRAM → full waveform.
        epdSetGRAMWindow(0, 12, disp_width, 184);
        display->setFullWindow();

        display->firstPage();
        do {
            display->fillScreen(GxEPD_WHITE);
            drawContent();
        } while (display->nextPage());

        triggerEpdRefresh(false);
    } else {
        renderPartialWithWhiteout(drawContent);
    }
}

// ── Default layout: header row + bottom status bar ──
void drawDefaultLayout() {
    bool full = pendingFullRefresh();  // true on mode switch, false for normal updates

    auto drawContent = []() {
        char chan_sf[20];
        if (current_mode == "PTT") {
            snprintf(chan_sf, sizeof(chan_sf), "chn:%c %dbps", channels[deviceSettings.channel_idx], getBitrateFromIndex(deviceSettings.bitrate_idx));
        } else {
            snprintf(chan_sf, sizeof(chan_sf), "chn:%c spf:%d", channels[deviceSettings.channel_idx], deviceSettings.spreading_factor);
        }

        drawHeaderRow(current_mode, chan_sf);
        drawBottomStatusbar();
    };

    renderPageLoop(drawContent, full);
}

// ── Per-mode: BEACON — peer roster + distance (Split Rows layout) ──
void drawBeaconLayout() {
    bool full = pendingFullRefresh();

    auto drawContent = []() {
        display->fillScreen(GxEPD_WHITE);

        char chan_sf[20];
        snprintf(chan_sf, sizeof(chan_sf), "chn:%c spf:%d", channels[deviceSettings.channel_idx], deviceSettings.spreading_factor);
        drawHeaderRow("BEACON", chan_sf);

        // Peer name and distance area (rows 2-5)
        display->fillRect(12, 32, disp_width - 24, 80, GxEPD_WHITE);

        double bestDist = beacon_display_dist;
        String bestName = beacon_display_name;

        if (bestDist >= 0 && peerRosterCount > 0) {
            // Show closest peer with distance
            display->setFont(&FreeMonoBold9pt7b);
            int dy = 36;

            // Peer name line
            display->fillRect(12, dy, disp_width - 24, 18, GxEPD_BLACK);
            display->setCursor(16, dy + 12);
            display->setTextColor(GxEPD_WHITE);

            // Truncate long call signs for display
            char nameDisplay[20];
            String bn = bestName;
            if (bn.length() > 18) {
                bn = bn.substring(0, 17) + "...";
            }
            strncpy(nameDisplay, bn.c_str(), sizeof(nameDisplay));
            display->print(nameDisplay);

            dy += 22;

            // Distance line
            display->fillRect(12, dy, disp_width - 24, 18, GxEPD_WHITE);
            display->setCursor(16, dy + 12);
            display->setTextColor(GxEPD_BLACK);

            if (bestDist < 1000) {
                snprintf(chan_sf, sizeof(chan_sf), "%.0fm", bestDist);
            } else {
                snprintf(chan_sf, sizeof(chan_sf), "%.1fkm", bestDist / 1000.0);
            }
            display->print(chan_sf);
        } else {
            drawSecondaryRow("No peer loc", 36);
        }

        // Peer count on a separate line
        display->fillRect(12, 74, disp_width - 24, 14, GxEPD_WHITE);
        char buf[30];
        snprintf(buf, sizeof(buf), "%d peers", peerRosterCount);
        display->setCursor(16, 74 + 10);
        display->setTextColor(GxEPD_BLACK);
        display->print(buf);

        // Peer roster list starting at y=90 (lines 7-12+)
        int rosterStartY = 88;
        int maxRows = (disp_height - 104) / 12;  // rows after header/status
        int rosterIdx = 0;

        for (int i = 0; i < peerRosterCount && rosterIdx < maxRows; i++) {
            float dist = peerRoster[i].distanceM;
            const char* displayName = peerRoster[i].callSign[0] != '\0' ? peerRoster[i].callSign : peerRoster[i].deviceId.c_str();
            int batt = peerRoster[i].battery;

            // Clear the row background
            display->fillRect(12, rosterStartY + (rosterIdx * 12), disp_width - 24, 12, GxEPD_WHITE);

            char lineBuf[50];
            snprintf(lineBuf, sizeof(lineBuf), "%s", displayName);

            if (dist > 0 && dist < 1000) {
                char dBuf[20];
                snprintf(dBuf, sizeof(dBuf), " %.0fm", dist);
                strncat(lineBuf, dBuf, sizeof(lineBuf) - strlen(lineBuf) - 1);
            } else if (dist >= 1000) {
                char dBuf[20];
                snprintf(dBuf, sizeof(dBuf), " %.1fkm", dist / 1000.0);
                strncat(lineBuf, dBuf, sizeof(lineBuf) - strlen(lineBuf) - 1);
            }

            if (batt > 0) {
                char bBuf[12];
                snprintf(bBuf, sizeof(bBuf), " %d%%", batt);
                strncat(lineBuf, bBuf, sizeof(lineBuf) - strlen(lineBuf) - 1);
            }

            display->setCursor(16, rosterStartY + (rosterIdx * 12) + 9);
            display->setTextColor(GxEPD_BLACK);
            display->print(lineBuf);

            rosterIdx++;
        }

        // Clear remaining roster rows
        for (int i = rosterIdx; i < maxRows; i++) {
            display->fillRect(12, rosterStartY + (i * 12), disp_width - 24, 12, GxEPD_WHITE);
        }

        drawBottomStatusbar();
    };

    renderPageLoop(drawContent, full);
}

// ── Per-mode: RANGE — distance + role card ──
void drawRangeLayout() {
    bool full = pendingFullRefresh();

    auto drawContent = []() {
        display->fillScreen(GxEPD_WHITE);

        char chan_sf[20];
        snprintf(chan_sf, sizeof(chan_sf), "chn:%c spf:%d", channels[deviceSettings.channel_idx], deviceSettings.spreading_factor);
        drawHeaderRow("RANGE", chan_sf);

        display->fillRect(12, 32, disp_width - 24, 80, GxEPD_WHITE);

        int row = 36;

        // Role indicator
        display->fillRect(12, row, disp_width - 24, 16, GxEPD_BLACK);
        display->setCursor(16, row + 11);
        display->setTextColor(GxEPD_WHITE);
        display->print(range_role_sender ? "Role: SENDER" : "Role: RECEIVER");

        if (gps_status != GPS_LOC) {
            row += 20;
            char disp_msg[32];
            snprintf(disp_msg, sizeof(disp_msg), "GPS: %.1f", gps_hdop);
            display->fillRect(12, row, disp_width - 24, 14, GxEPD_WHITE);
            display->setCursor(16, row + 10);
            display->setTextColor(GxEPD_BLACK);
            display->print(disp_msg);
        } else {
            // Coordinates
            row += 20;
            char disp_msg[32];
            snprintf(disp_msg, sizeof(disp_msg), "%.6f", gps_latitude);
            display->fillRect(12, row, disp_width - 24, 14, GxEPD_WHITE);
            display->setCursor(16, row + 10);
            display->setTextColor(GxEPD_BLACK);
            display->print(disp_msg);

            row += 18;
            snprintf(disp_msg, sizeof(disp_msg), "%.6f", gps_longitude);
            display->fillRect(12, row, disp_width - 24, 14, GxEPD_WHITE);
            display->setCursor(16, row + 10);
            display->setTextColor(GxEPD_BLACK);
            display->print(disp_msg);

            // Distance readings
            row += 20;
            snprintf(disp_msg, sizeof(disp_msg), "Rng:%.1fm", range_stable_dist > 0 ? range_stable_dist : 0);
            display->fillRect(12, row, disp_width - 24, 14, GxEPD_WHITE);
            display->setCursor(16, row + 10);
            display->setTextColor(GxEPD_BLACK);
            display->print(disp_msg);

            row += 18;
            snprintf(disp_msg, sizeof(disp_msg), "Max:%.1fm", range_max_dist > 0 ? range_max_dist : 0);
            display->fillRect(12, row, disp_width - 24, 14, GxEPD_WHITE);
            display->setCursor(16, row + 10);
            display->setTextColor(GxEPD_BLACK);
            display->print(disp_msg);

            // Packet loss stats
            row += 18;
            snprintf(disp_msg, sizeof(disp_msg), "PLoss:%d/%d", range_total_pckt_loss, range_consecutive_ok);
            display->fillRect(12, row, disp_width - 24, 14, GxEPD_WHITE);
            display->setCursor(16, row + 10);
            display->setTextColor(GxEPD_BLACK);
            display->print(disp_msg);
        }

        drawBottomStatusbar();
    };

    renderPageLoop(drawContent, full);
}

// ── Per-mode: PTT Big Card TX/RX state block ──
void drawPttLayout() {
    bool full = pendingFullRefresh();

    auto drawContent = []() {
        display->fillScreen(GxEPD_WHITE);

        // Determine PTT state
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

        char chan_sf[20];
        snprintf(chan_sf, sizeof(chan_sf), "chn:%c %dbps", channels[deviceSettings.channel_idx], getBitrateFromIndex(deviceSettings.bitrate_idx));
        drawHeaderRow("PTT", chan_sf);

        // State block at y=48 — bordered rect (176x82 pixels)
        int blk_x = 12, blk_y = 48, blk_w = 176, blk_h = 82;

        // Fill background first (critical for e-ink inversion safety)
        display->fillRect(blk_x, blk_y, blk_w, blk_h, bg_color);

        // Draw border on top of filled area
        display->drawRect(blk_x, blk_y, blk_w, blk_h, GxEPD_BLACK);

        // State text centered in block
        int title_line = blk_y + 22;
        display->setFont(&FreeMonoBold12pt7b);
        display->setCursor(blk_x + 6, title_line + 10);
        display->setTextColor(text_color);
        display->print(state_text);

        // Activity indicator dots below state text (3 circles)
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

        // Channel / bitrate info below state block
        display->setFont(&FreeMonoBold9pt7b);
        display->setCursor(12, blk_y + blk_h + 20);
        display->setTextColor(GxEPD_BLACK);

        char chan_detail[32];
        snprintf(chan_detail, sizeof(chan_detail), "chn:%c %dbps",
                 channels[deviceSettings.channel_idx],
                 getBitrateFromIndex(deviceSettings.bitrate_idx));
        display->print(chan_detail);

        // Signal metrics on next row
        display->setCursor(12, blk_y + blk_h + 36);

        char sig_buf[50];
        float rssi_val = (radio != nullptr) ? radio->getRSSI() : -127.0f;
        float snr_val = (radio != nullptr) ? radio->getSNR() : 0.0f;
        snprintf(sig_buf, sizeof(sig_buf), "SNR:%.1fdB RSSI:%.0fdBm", snr_val, rssi_val);
        display->print(sig_buf);

        drawBottomStatusbar();
    };

    renderPageLoop(drawContent, full);
}

// ── Per-mode: TXT single message (from packet) ──
void drawTxtSingleLayout() {
    markScreenDirty();
    // Guard: never render TXT layout when not in TXT mode (prevents TST flash artifact)
    extern const char* current_mode;
    if (strcmp(current_mode, "TXT") != 0) {
        return;
    }

    bool full = pendingFullRefresh();

    auto drawContent = []() {
        display->fillScreen(GxEPD_WHITE);

        char chan_sf[20];
        snprintf(chan_sf, sizeof(chan_sf), "chn:%c spf:%d", channels[deviceSettings.channel_idx], deviceSettings.spreading_factor);
        drawHeaderRow("TXT", chan_sf);

        // Show latest message content
        display->fillRect(12, 32, disp_width - 24, 80, GxEPD_WHITE);

        char buf[60];
        snprintf(buf, sizeof(buf), "%d msg(s)", txtInboxMsgCount > 0 ? txtInboxMsgCount : inboxCount());
        display->setCursor(16, 36 + 10);
        display->setFont(&FreeMonoBold9pt7b);
        display->setTextColor(GxEPD_BLACK);
        display->print(buf);

        // Show latest message preview
        if (txtInboxMsgCount > 0) {
            uint8_t count = inboxCount();
            if (count > 0) {
                inboxGetMessage(count - 1, buf, sizeof(buf));
                display->fillRect(12, 56, disp_width - 24, 14, GxEPD_WHITE);
                display->setCursor(16, 56 + 10);
                display->print(buf);
            } else {
                drawSecondaryRow("No messages", 56);
            }
        } else {
            drawSecondaryRow("No messages yet", 56);
        }

        // Show peer/peer channel liveness status at y=130 (like BEACON's distance readout)
        display->fillRect(12, 128, disp_width - 24, 14, GxEPD_WHITE);
        extern bool isPeerAlive();
        if (isPeerAlive()) {
            drawPrimaryValue("\u2713 On channel", &FreeMonoBold9pt7b, 16, 128);
        } else {
            drawSecondaryRow("No one on channel", 130);
        }

        drawBottomStatusbar();
    };

    renderPageLoop(drawContent, full);
}

// ── Per-mode: TXT inbox (from txtShowInbox view) ──
void drawTxtInboxLayout() {
    markScreenDirty();
    bool full = pendingFullRefresh();

    auto drawContent = []() {
        display->fillScreen(GxEPD_WHITE);

        char chan_sf[20];
        snprintf(chan_sf, sizeof(chan_sf), "chn:%c spf:%d", channels[deviceSettings.channel_idx], deviceSettings.spreading_factor);
        drawHeaderRow("TXT", chan_sf);

        // Inbox page header
        uint8_t msg_count = txtInboxMsgCount > 0 ? txtInboxMsgCount : inboxCount();

        display->fillRect(12, 32, disp_width - 24, 16, GxEPD_BLACK);
        char page_header[32];
        snprintf(page_header, sizeof(page_header), "P %d/%d (%d msgs)",
                 txtInboxScrollPage + 1, (msg_count + 15) / 16, msg_count);
        display->setCursor(16, 32 + 11);
        display->setFont(&FreeMonoBold9pt7b);
        display->setTextColor(GxEPD_WHITE);
        display->print(page_header);

        // Message list rows (starting at y=50)
        int rosterStartY = 50;
        int maxRows = (disp_height - 104) / 10;  // leave room for peer status line

        char inbox_msg_buf[INBOX_MAX_MSG_LEN + 1];

        for (int i = 0; i < msg_count && ((i - txtInboxScrollPage * 16) < maxRows) && (i - txtInboxScrollPage * 16) >= 0; i++) {
            int rowIdx = i - txtInboxScrollPage * 16;
            if (i < txtInboxScrollPage * 16 || i >= (txtInboxScrollPage + 1) * 16) continue;

            inboxGetMessage(i, inbox_msg_buf, sizeof(inbox_msg_buf));
            display->fillRect(12, rosterStartY + (rowIdx * 10), disp_width - 24, 10, GxEPD_WHITE);
            display->setCursor(16, rosterStartY + (rowIdx * 10) + 8);
            display->setFont(&FreeMonoBold9pt7b);
            display->setTextColor(GxEPD_BLACK);
            display->print(inbox_msg_buf);
        }

        // Peer/peer channel liveness status at bottom of message list (above statusbar)
        display->fillRect(12, 160, disp_width - 24, 14, GxEPD_WHITE);
        extern bool isPeerAlive();
        if (isPeerAlive()) {
            drawPrimaryValue("\u2713 On channel", &FreeMonoBold9pt7b, 16, 160);
        } else {
            drawSecondaryRow("No one on channel", 162);
        }

        drawBottomStatusbar();
    };

    renderPageLoop(drawContent, full);
}

// ── Per-mode: TST — Sent/Recv dashboard ──
void drawTstLayout() {
    markScreenDirty();
    bool full = pendingFullRefresh();

    auto drawContent = []() {
        display->fillScreen(GxEPD_WHITE);

        char chan_sf[20];
        snprintf(chan_sf, sizeof(chan_sf), "chn:%c spf:%d", channels[deviceSettings.channel_idx], deviceSettings.spreading_factor);
        drawHeaderRow("TST", chan_sf);

        display->fillRect(12, 32, disp_width - 24, 80, GxEPD_WHITE);

        char buf[40];

        int sent = test_message_counter;
        int recv = pckt_count;
        snprintf(buf, sizeof(buf), "Sent: %d", sent);
        display->setCursor(16, 36 + 10);
        display->setFont(&FreeMonoBold9pt7b);
        display->setTextColor(GxEPD_BLACK);
        display->print(buf);

        snprintf(buf, sizeof(buf), "Recv: %d", recv);
        display->setCursor(16, 56 + 10);
        display->setFont(&FreeMonoBold9pt7b);
        display->setTextColor(GxEPD_BLACK);
        display->print(buf);

        drawBottomStatusbar();
    };

    renderPageLoop(drawContent, full);
}

// ── Per-mode: PONG — state + RTT display ──
void drawPongLayout() {
    markScreenDirty();
    bool full = pendingFullRefresh();

    auto drawContent = []() {
        display->fillScreen(GxEPD_WHITE);

        char chan_sf[20];
        snprintf(chan_sf, sizeof(chan_sf), "chn:%c spf:%d", channels[deviceSettings.channel_idx], deviceSettings.spreading_factor);
        drawHeaderRow("PONG", chan_sf);

        display->fillRect(12, 32, disp_width - 24, 80, GxEPD_WHITE);

        int row = 36;
        char buf[40];

        const char* state_str = "idle";
        if (layout_state.pong_state == 1) state_str = "sending";
        else if (layout_state.pong_state == 2) state_str = "received";

        snprintf(buf, sizeof(buf), "State: %s", state_str);
        display->setCursor(16, row + 10);
        display->setFont(&FreeMonoBold9pt7b);
        display->setTextColor(GxEPD_BLACK);
        display->print(buf);

        row += 20;
        int rtt = layout_state.pong_rtt_ms;
        if (rtt > 0) {
            snprintf(buf, sizeof(buf), "RTT: %d ms", rtt);
            display->setCursor(16, row + 10);
            display->print(buf);
        } else {
            drawSecondaryRow("RTT: --- ms", row);
        }

        drawBottomStatusbar();
    };

    renderPageLoop(drawContent, full);
}

// ── Per-mode: SCAN — scan progress + top channels ──
void drawScanLayout() {
    markScreenDirty();
    bool full = pendingFullRefresh();

    // Sync scan state into layout_state for this render pass
    LayoutState& S = layout_state;
    S.scan_progress_pct = scanning ? (int)((currentFrequency - startFreq) / (endFreq - startFreq) * 100) : 0;
    if (scanning) {
        snprintf(S.scan_current_freq, sizeof(S.scan_current_freq), "%.2f", currentFrequency);
    } else {
        snprintf(S.scan_current_freq, sizeof(S.scan_current_freq), "---");
    }

    auto drawContent = []() {
        display->fillScreen(GxEPD_WHITE);

        char chan_sf[20];
        snprintf(chan_sf, sizeof(chan_sf), "chn:%c spf:%d", channels[deviceSettings.channel_idx], deviceSettings.spreading_factor);
        drawHeaderRow("SCAN", chan_sf);

        // ── Current frequency (rows 2-3) ──
        display->fillRect(12, 32, disp_width - 24, 18, GxEPD_WHITE);
        display->setFont(&FreeMonoBold9pt7b);
        display->setCursor(16, 32 + 12);
        display->setTextColor(GxEPD_BLACK);

        char freqBuf[20];
        if (strlen(layout_state.scan_current_freq) > 0 && layout_state.scan_current_freq[0] != '-') {
            snprintf(freqBuf, sizeof(freqBuf), "F:%.2fMHz", currentFrequency);
        } else {
            snprintf(freqBuf, sizeof(freqBuf), "F:---.--MHz");
        }
        display->print(freqBuf);

        // ── Progress bar (rows 4-5) ──
        int row = 52;
        if (layout_state.scan_progress_pct > 0) {
            drawSecondaryRow("Progress", row);
            row += 14;

            // Bar background
            display->fillRect(16, row + 4, disp_width - 32, 8, GxEPD_BLACK);

            // Filled portion
            int bar_w = layout_state.scan_progress_pct * (disp_width - 40) / 100;
            if (bar_w > 0 && bar_w <= (disp_width - 40)) {
                display->fillRect(16, row + 4, bar_w, 8, GxEPD_WHITE);
            }

            char pctBuf[10];
            snprintf(pctBuf, sizeof(pctBuf), "%d%%", layout_state.scan_progress_pct);
            display->setCursor(16 + (disp_width - 32) / 2 - strlen(pctBuf) * 4, row + 10);
            display->print(pctBuf);
        } else {
            drawSecondaryRow("Progress: ---%", row);
        }

        // ── Top channels list (rows 6+) ──
        int listStartY = row + 24;
        int channelCount = layout_state.scan_channel_count;
        const LayoutState::ScanChannel* channels_data = layout_state.scan_channels;

        // Header for the list
        display->fillRect(12, listStartY, disp_width - 24, 14, GxEPD_BLACK);
        display->setFont(&FreeMonoBold9pt7b);
        display->setCursor(16, listStartY + 10);
        display->setTextColor(GxEPD_WHITE);
        display->print("Top Channels");

        int rowIdx = 0;
        int itemY = listStartY + 16;
        int maxItems = (disp_height - itemY - 32) / 12;  // leave room for status bar
        if (maxItems > 10) maxItems = 10;

        for (uint8_t i = 0; i < channelCount && rowIdx < maxItems; i++) {
            const auto& ch = channels_data[i];

            // Clear row background
            display->fillRect(12, itemY + (rowIdx * 12), disp_width - 24, 12, GxEPD_WHITE);

            char lineBuf[40];
            snprintf(lineBuf, sizeof(lineBuf), "%.2f Q%d %.0fdBm", ch.frequency, ch.quality, ch.rssi);

            display->setCursor(16, itemY + (rowIdx * 12) + 9);
            display->setTextColor(GxEPD_BLACK);
            display->print(lineBuf);

            rowIdx++;
        }

        // Clear remaining rows
        for (int i = rowIdx; i < maxItems; i++) {
            display->fillRect(12, itemY + (i * 12), disp_width - 24, 12, GxEPD_WHITE);
        }

        drawBottomStatusbar();
    };

    renderPageLoop(drawContent, full);
}

// ── Per-mode: RAW — raw packet hex display ──
void drawRawLayout() {
    markScreenDirty();
    bool full = pendingFullRefresh();

    auto drawContent = []() {
        display->fillScreen(GxEPD_WHITE);

        char chan_sf[20];
        snprintf(chan_sf, sizeof(chan_sf), "chn:%c spf:%d", channels[deviceSettings.channel_idx], deviceSettings.spreading_factor);
        drawHeaderRow("RAW", chan_sf);

        display->fillRect(12, 32, disp_width - 24, 80, GxEPD_WHITE);

        int row = 36;
        char buf[60];

        if (strlen(layout_state.raw_hex_line1) > 0) {
            display->setCursor(16, row + 10);
            display->setFont(&FreeMonoBold9pt7b);
            display->setTextColor(GxEPD_BLACK);
            display->print(layout_state.raw_hex_line1);
        } else {
            drawSecondaryRow("---", row);
        }

        drawBottomStatusbar();
    };

    renderPageLoop(drawContent, full);
}

// ── Per-mode: SETTINGS — current setting name + value card ──
void drawSettingsLayout() {
    markScreenDirty();
    bool full = pendingFullRefresh();

    auto drawContent = []() {
        display->fillScreen(GxEPD_WHITE);

        // Header row with settings gear icon
        char chan_sf[20];
        snprintf(chan_sf, sizeof(chan_sf), "chn:%c spf:%d", channels[deviceSettings.channel_idx], deviceSettings.spreading_factor);
        drawModeIcon("SETTINGS");

        int name_x = 24;
        display->setFont(&FreeMonoBold9pt7b);
        display->fillRect(name_x, disp_top_margin, 100, 16, GxEPD_BLACK);
        display->setCursor(name_x + 4, disp_top_margin + 11);
        display->setTextColor(GxEPD_WHITE);
        display->print("SETTINGS");

        int channel_x = disp_width - 90;
        display->fillRect(channel_x, disp_top_margin, 86, 16, GxEPD_BLACK);
        display->setCursor(channel_x + 4, disp_top_margin + 11);
        display->setTextColor(GxEPD_WHITE);
        display->print(chan_sf);

        display->setTextColor(GxEPD_BLACK);

        // Body area: setting label + value in bordered card
        int card_y = 32;
        int card_h = 110;

        // Card background with border
        display->fillRect(12, card_y, disp_width - 24, card_h, GxEPD_WHITE);
        display->drawRect(12, card_y, disp_width - 24, card_h, GxEPD_BLACK);

        // Determine current setting name and value
        const char* setting_name = nullptr;
        char setting_value[64] = "";

        switch (setting_idx) {
            case SPREADING_FACTOR:
                setting_name = "Spreading Factor";
                snprintf(setting_value, sizeof(setting_value), "SF: %d", deviceSettings.spreading_factor);
                break;
            case CHANNEL:
                setting_name = "Channel";
                snprintf(setting_value, sizeof(setting_value), "%c", channels[deviceSettings.channel_idx]);
                break;
            case BITRATE:
                setting_name = "Bitrate";
                snprintf(setting_value, sizeof(setting_value), "%d bps", getBitrateFromIndex(deviceSettings.bitrate_idx));
                break;
            case BACKLIGHT:
                setting_name = "Backlight";
                strncpy(setting_value, deviceSettings.backlight ? "On" : "Off", sizeof(setting_value));
                break;
            case VOLUME:
                setting_name = "Volume";
                snprintf(setting_value, sizeof(setting_value), "%d", deviceSettings.volume_level);
                break;
            case HOURS: {
                RTC_Date dt = rtc.getDateTime();
                setting_name = "Hour";
                char time_buf[9];
                snprintf(time_buf, sizeof(time_buf), "%02d:%02d:%02d", dt.hour, dt.minute, dt.second);
                strncpy(setting_value, time_buf, sizeof(setting_value));
                break;
            }
            case MINUTES: {
                RTC_Date dt = rtc.getDateTime();
                setting_name = "Minute";
                char time_buf[9];
                snprintf(time_buf, sizeof(time_buf), "%02d:%02d:%02d", dt.hour, dt.minute, dt.second);
                strncpy(setting_value, time_buf, sizeof(setting_value));
                break;
            }
            case SECONDS: {
                RTC_Date dt = rtc.getDateTime();
                setting_name = "Second";
                char time_buf[9];
                snprintf(time_buf, sizeof(time_buf), "%02d:%02d:%02d", dt.hour, dt.minute, dt.second);
                strncpy(setting_value, time_buf, sizeof(setting_value));
                break;
            }
            case BANDWIDTH: {
                float bw_khz = deviceSettings.bandwidth_idx / 1000.0;
                setting_name = "Bandwidth";
                snprintf(setting_value, sizeof(setting_value), "%.2f kHz", bw_khz);
                break;
            }
            case CODING_RATE:
                setting_name = "Coding Rate";
                snprintf(setting_value, sizeof(setting_value), "1/%d", deviceSettings.coding_rate_idx);
                break;
            case FREQUENCY_HOPPING:
                setting_name = "Freq Hopping";
                strncpy(setting_value, deviceSettings.frequency_hopping_enabled ? "Enabled" : "Disabled", sizeof(setting_value));
                break;
        }

        // Draw setting name (top of card)
        display->setFont(&FreeMonoBold9pt7b);
        int name_y = card_y + 12;
        display->fillRect(16, name_y, disp_width - 32, 18, GxEPD_BLACK);
        display->setCursor(20, name_y + 12);
        display->setTextColor(GxEPD_WHITE);
        display->print(setting_name);

        // Draw setting value (bottom of card)
        int val_y = card_y + 40;
        display->setFont(&FreeMonoBold12pt7b);
        int text_w = strlen(setting_value) * 8;
        int cx = (disp_width - text_w) / 2;
        display->setCursor(cx, val_y + 10);
        display->setTextColor(GxEPD_BLACK);
        display->print(setting_value);

        // Draw instruction hint at bottom of card
        int instr_y = card_y + card_h - 24;
        display->setFont(&FreeMonoBold9pt7b);
        display->fillRect(16, instr_y, disp_width - 32, 18, GxEPD_WHITE);
        display->setCursor(20, instr_y + 12);
        display->setTextColor(GxEPD_BLACK);
        display->print("MODE: cycle");

        drawBottomStatusbar();
    };

    renderPageLoop(drawContent, full);
}

// ── Per-mode: WP — Lat/Lon + broadcast ──
void drawWpLayout() {
    markScreenDirty();
    bool full = pendingFullRefresh();

    auto drawContent = []() {
        display->fillScreen(GxEPD_WHITE);

        char chan_sf[20];
        snprintf(chan_sf, sizeof(chan_sf), "chn:%c spf:%d", channels[deviceSettings.channel_idx], deviceSettings.spreading_factor);
        drawHeaderRow("WP", chan_sf);

        display->fillRect(12, 32, disp_width - 24, 80, GxEPD_WHITE);

        int row = 36;
        char buf[60];

        snprintf(buf, sizeof(buf), "Lat: %.6f", layout_state.wp_lat);
        display->setCursor(16, row + 10);
        display->setFont(&FreeMonoBold9pt7b);
        display->setTextColor(GxEPD_BLACK);
        display->print(buf);

        row += 20;
        snprintf(buf, sizeof(buf), "Lon: %.6f", layout_state.wp_lon);
        display->setCursor(16, row + 10);
        display->print(buf);

        row += 20;
        if (layout_state.wp_broadcasting) {
            snprintf(buf, sizeof(buf), "Broadcast %ds", layout_state.wp_bcast_remaining_s);
            display->setCursor(16, row + 10);
            display->print(buf);
        } else if (layout_state.wp_alt > 0) {
            snprintf(buf, sizeof(buf), "Alt: %.1fm", layout_state.wp_alt);
            display->setCursor(16, row + 10);
            display->print(buf);
        }

        drawBottomStatusbar();
    };

    renderPageLoop(drawContent, full);
}
