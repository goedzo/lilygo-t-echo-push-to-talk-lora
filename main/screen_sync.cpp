#include "screen_sync.h"
#include "app_modes.h"
#include "display_layout.h"
#include "battery.h"
#include "gps.h"
#include "lora.h"
#include "ble.h"
#include "settings.h"
#include "text_inbox.h"
#include "scan.h"

#include <Arduino.h>

// Forward declarations for functions defined in ble.cpp (not in ble.h)
extern void sendSerialToApp(const String& msg);
extern void sendSerialToAppLn(const String& msg);

// Extern declarations for globals defined in app_modes.cpp
    extern double beacon_display_dist;
    extern String beacon_display_name;
    extern uint32_t sendTestMessageTimer;
    extern int pckt_count;
    extern int test_message_counter;
    extern int rcv_test_message_counter;
    extern bool range_role_sender;
    extern int  range_last_count;
    extern int  range_consecutive_ok;
    extern int  range_total_pckt_loss;
    extern double range_max_dist;
    extern double range_stable_dist;
    extern double range_home_lat;
    extern double range_home_long;

    // Extern declarations for scan.cpp globals
    extern ChannelResult topChannels[];
    extern int peerRosterCount;

// Payload format (JSON-like, compact):
// LINE:S|H:{channel_sf}|C:<content_fields>|S:{freq}|T:{time}|G:{sats}|B:{batt}%|I:gpst=ok,bat=N
// SCAN mode content fields: s{pct}@{current_freq},{ch1_f}{ch1_r},...  (f=freq*10, r=rssi*10)

static uint32_t last_sync_ms = 0;
static const uint32_t SYNC_INTERVAL_MS = 1000; // Throttle: max 1 update per second
static volatile bool screen_dirty = false;
static int8_t last_batt_idx = -1;

void markScreenDirty() { screen_dirty = true; }

void sendScreenSync() {
    uint32_t now = millis();
    if (now - last_sync_ms < SYNC_INTERVAL_MS) return;
    last_sync_ms = now;
    screen_dirty = false;

    if (!display) return;

    char buf[SYNC_MAX_PAYLOAD];
    int off = 0;

    // Header: mode + channel/SF info
    char chan_sf[32];
    if (strcmp(current_mode, "PTT") == 0) {
        snprintf(chan_sf, sizeof(chan_sf), "chn:%c %dbps", channels[deviceSettings.channel_idx], getBitrateFromIndex(deviceSettings.bitrate_idx));
    } else {
        snprintf(chan_sf, sizeof(chan_sf), "chn:%c spf:%d", channels[deviceSettings.channel_idx], deviceSettings.spreading_factor);
    }

    off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "LINE:S|M:%s|H:%s|C:", current_mode, chan_sf);

    // Per-mode content fields (app maps these to screen rows)
    if (strcmp(current_mode, "BEACON") == 0) {
        const char* pn = "---";
        if (beacon_display_name.length() > 0 && beacon_display_name.length() < 64) {
            // Safe access: verify String is not in a corrupted state
            static unsigned char tmpName[64];
            beacon_display_name.getBytes(tmpName, sizeof(tmpName));
            if (tmpName[0] != '\0') {
                pn = (const char*)tmpName;
            }
        }
        off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "peer_name:%s,", pn);
        off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "peer_dist:%.0fm,", (beacon_display_dist >= 0) ? beacon_display_dist : -1);
        off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "roster_count:%d,", peerRosterCount);

        int roster_rows = 0;
        // Guard against corrupted peerRosterCount causing out-of-bounds read
        int max_roster = peerRosterCount;
        if (max_roster < 0 || max_roster > 50) {
            sendSerialToAppLn(F("ERR:sendScreenSync corrupted rosterCount="));
            sendSerialToAppLn(String(max_roster));
            max_roster = 0;
        }
        for (int i = 0; i < max_roster && roster_rows < 5; i++) {
            const char* dn = peerRoster[i].callSign[0] != '\0' ? peerRoster[i].callSign : peerRoster[i].deviceId.c_str();
            float dist = peerRoster[i].distanceM;
            int batt = peerRoster[i].battery;

            off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "r%d_n=%s,", roster_rows, dn);

            if (dist > 0 && dist < 1000) {
                off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "r%d_d=%.0fm,", roster_rows, dist);
            } else if (dist >= 1000) {
                off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "r%d_d=%.1fkm,", roster_rows, dist / 1000.0);
            } else {
                off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "r%d_d=???,", roster_rows);
            }

            if (batt > 0) {
                off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "r%d_b=%d%%,", roster_rows, batt);
            } else {
                off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "r%d_b=-%%,", roster_rows);
            }

            roster_rows++;
        }
    }
    else if (strcmp(current_mode, "RANGE") == 0) {
        off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "range_role:%s,", range_role_sender ? "Sender" : "Receiver");
        off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "range_last_count:%d,", range_last_count);
        off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "range_consecutive_ok:%d,", range_consecutive_ok);
        off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "range_total_loss:%d,", range_total_pckt_loss);
        off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "range_stable_dist:%.1fm,", range_stable_dist);
        off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "range_max_dist:%.1fm,", range_max_dist);
        off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "home_lat:%.6f,", range_home_lat);
        off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "home_lon:%.6f,", range_home_long);

        if (gps_status == GPS_LOC) {
            off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "curr_lat:%.6f,", gps_latitude);
            off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "curr_lon:%.6f,", gps_longitude);
        }
    }
    else if (strcmp(current_mode, "PTT") == 0) {
        off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "ptt_state:%s,", layout_state.ptt_sending ? "TX" : (layout_state.ptt_receiving ? "RX" : "Idle"));
    }
    else if (strcmp(current_mode, "TXT") == 0) {
        uint8_t msg_count = inboxCount();
        off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "txt_inbox_count:%d,", msg_count);
        off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "txt_show_inbox:%d,", txtShowInbox ? 1 : 0);
        off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "txt_scroll_page:%d,", txtInboxScrollPage);

        if (!txtShowInbox && msg_count > 0) {
            uint8_t out_len = 0;
            bool out_trunc = false;
            uint8_t out_sender[4] = {0};
            const char* raw = inboxGet(0, out_len, out_trunc, out_sender);
            if (raw && out_len > 0) {
                static char sanitized[INBOX_MAX_MSG_LEN];
                int si = 0;
                for (uint8_t i = 0; i < out_len && si < sizeof(sanitized) - 1; i++) {
                    if (raw[i] == ',') { sanitized[si++] = ':'; }
                    else { sanitized[si++] = raw[i]; }
                }
                sanitized[si] = '\0';
                off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "txt_latest_msg=%s,", sanitized);
            }
        }
    }
    else if (strcmp(current_mode, "TST") == 0) {
        off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "tst_sent:%d,", test_message_counter);
        off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "tst_recv:%d,", rcv_test_message_counter);
    }
    else if (strcmp(current_mode, "PONG") == 0) {
        off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "pong_state:%d,", layout_state.pong_state);
        off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "pong_rtt_ms:%d,", layout_state.pong_rtt_ms);
    }
    else if (strcmp(current_mode, "SCAN") == 0) {
        // Compact scan format: "s{pct}@{freq}{ch1_f_r,ch2_f_r,...}~"
        // Each channel pair = "{f8digits}r{-3digits}" = 12 bytes (float encoded as fixed-point)
        off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "s%d@%.0f,", layout_state.scan_progress_pct, currentFrequency);
        for (int i = 0; i < 5 && topChannels[i].frequency > 0; i++) {
            // Encode frequency as integer MHz*10000 → 8 digits. RSSI * 10 → -3 digits (negative)
            int freq_10k = (int)(topChannels[i].frequency * 10);
            int rssi_10 = (int)(topChannels[i].rssi * 10);
            off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "%08d%d,", freq_10k, rssi_10);
        }
    }
    else if (strcmp(current_mode, "RAW") == 0) {
        off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "raw_count:%d,", pckt_count);
        off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "raw_last_rssi=%.1fdBm,", radio->getRSSI());
        off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "raw_last_snr=%.1fdB,", radio->getSNR());
    }
    else if (strcmp(current_mode, "WP") == 0) {
        off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "wp_label=%s,", layout_state.wp_label);
        off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "wp_lat=%.6f,", layout_state.wp_lat);
        off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "wp_lon=%.6f,", layout_state.wp_lon);
        off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "wp_alt=%.1fm,", layout_state.wp_alt);
        off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "wp_broadcasting:%d,", layout_state.wp_broadcasting ? 1 : 0);
        off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "wp_bcast_rem:%ds,", layout_state.wp_bcast_remaining_s);
    }

    // Status bar: freq, time, GPS sats, battery level
    char freq_str[16];
    snprintf(freq_str, sizeof(freq_str), "%.2f", currentFrequency);

    RTC_Date dateTime = rtc.getDateTime();
    char time_str[9];
    snprintf(time_str, sizeof(time_str), "%02d:%02d", dateTime.hour, dateTime.minute);

    uint8_t batt_pct = getBatteryPercentage();
    int bat_idx = 6;
    if (batt_pct > 90) bat_idx = 6;
    else if (batt_pct > 80) bat_idx = 5;
    else if (batt_pct > 60) bat_idx = 4;
    else if (batt_pct > 40) bat_idx = 3;
    else if (batt_pct > 20) bat_idx = 2;
    else if (batt_pct > 10) bat_idx = 1;
    else bat_idx = 0;

    // Only send battery data if icon level changed — avoids flooding from ADC noise
    bool batt_changed = (bat_idx != last_batt_idx);
    last_batt_idx = bat_idx;

    // Close content field and add status bar
    off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "|S:%s|T:%s|G:%d|B:%d%%|I:", freq_str, time_str, gps_satellites, batt_pct);

    // Icons: icon_name:value pairs (gpst=GPS state, bat=battery icon level)
    if (gps_status == GPS_LOC) {
        off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "gpst=ok,");
    } else {
        off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "gpst=no%,");
    }

    off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "bat=%d", bat_idx);

    buf[off] = '\0';

    if (off >= SYNC_MAX_PAYLOAD) {
        sendSerialToAppLn(F("ERR:sendScreenSync buffer overrun off="));
        sendSerialToAppLn(String(off));
        return;
    }
    
    // Verify queue has room for this large message before attempting to send
    int pending = getPendingNotificationCount();
    if (pending > 8) {
        return;
    }

    // Only push if something OTHER than battery changed.
    // Battery ADC noise causes tiny percentage swings (134→135%) that map to the same icon level.
    // Time gives one update/minute anyway — no need to flood on battery-only changes.
    // EXCEPT: scan mode always needs updates (progress bar and top channels change continuously)
    if (!batt_changed && strcmp(current_mode, "SCAN") != 0) {
        last_batt_idx = bat_idx;
        last_sync_ms = now;
        return;
    }

    // Send via BLE
    sendNotificationToApp(buf);
}

// Only push screen sync if display state changed since last notification
void sendScreenSyncIfDirty() {
    if (!screen_dirty) return;
    
    // Only sync when BLE is connected — prevents queue filling during boot with no app
    extern int getPendingNotificationCount();
    extern bool isBleConnected();
    extern uint32_t ble_connect_stall_until;
    if (!isBleConnected()) {
        screen_dirty = false;  // Clear dirty flag to prevent flood on next connect
        return;
    }

    // Don't sync during connection stall — nothing can receive the notification
    if (millis() < ble_connect_stall_until) {
        screen_dirty = false;  // Clear dirty flag so we don't rebuild stale content
        return;
    }

    uint32_t now = millis();
    if (now - last_sync_ms < SYNC_INTERVAL_MS) return;
    
    // Throttle: even dirty updates max out at 1/sec to avoid flooding SoftDevice
    sendScreenSync();
}

// Forced screen sync — bypasses all throttling, battery-change checks, and intervals.
// Called in response to GETSCREEN from the companion app.
void sendScreenSyncForced() {
    uint32_t now = millis();
    
    if (!display) return;

    // Must be connected — there's nowhere to send otherwise
    extern bool isBleConnected();
    extern uint32_t ble_connect_stall_until;
    extern void sendSerialToAppLn(const String& msg);
    if (!isBleConnected()) {
        sendSerialToAppLn(F("ERR:sendScreenSyncForced not connected"));
        return;
    }

    // During stall period, there's nothing to receive the notification
    if (now < ble_connect_stall_until) return;

    // Reset sync timestamp so we can immediately send even if just synced
    last_sync_ms = 0;
    
    // Force battery-change check to pass
    int8_t saved_batt_idx = last_batt_idx;
    last_batt_idx = -1;
    
    // Send via BLE — reuses existing sendScreenSync payload generation
    // but without any throttle or battery-change filtering
    
    // If a normal dirty sync is pending, clear it — the forced sync we're about to send
    // will carry the latest screen state anyway. The periodic sync will fire again next interval.
    if (screen_dirty) {
        screen_dirty = false;
    }
    
    extern void sendNotificationToApp(const char* message);
    int p = getPendingNotificationCount();
    if (p >= 14) {
        sendSerialToAppLn(F("ERR:sendScreenSyncForced queue full after drain"));
        last_batt_idx = saved_batt_idx;
        return;
    }

    // Build payload inline (same logic as sendScreenSync, but without throttle guards)
    char buf[SYNC_MAX_PAYLOAD];
    int off = 0;

    char chan_sf[32];
    if (strcmp(current_mode, "PTT") == 0) {
        snprintf(chan_sf, sizeof(chan_sf), "chn:%c %dbps", channels[deviceSettings.channel_idx], getBitrateFromIndex(deviceSettings.bitrate_idx));
    } else {
        snprintf(chan_sf, sizeof(chan_sf), "chn:%c spf:%d", channels[deviceSettings.channel_idx], deviceSettings.spreading_factor);
    }

    off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "LINE:S|M:%s|H:%s|C:", current_mode, chan_sf);

    if (strcmp(current_mode, "BEACON") == 0) {
        const char* pn = "---";
        if (beacon_display_name.length() > 0 && beacon_display_name.length() < 64) {
            static unsigned char tmpName[64];
            beacon_display_name.getBytes(tmpName, sizeof(tmpName));
            if (tmpName[0] != '\0') pn = (const char*)tmpName;
        }
        off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "peer_name:%s,", pn);
        off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "peer_dist:%.0fm,", (beacon_display_dist >= 0) ? beacon_display_dist : -1);
        off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "roster_count:%d,", peerRosterCount);
        int roster_rows = 0;
        int max_roster = peerRosterCount;
        if (max_roster < 0 || max_roster > 50) max_roster = 0;
        for (int i = 0; i < max_roster && roster_rows < 5; i++) {
            const char* dn = peerRoster[i].callSign[0] != '\0' ? peerRoster[i].callSign : peerRoster[i].deviceId.c_str();
            float dist = peerRoster[i].distanceM;
            int batt = peerRoster[i].battery;
            off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "r%d_n=%s,", roster_rows, dn);
            if (dist > 0 && dist < 1000) {
                off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "r%d_d=%.0fm,", roster_rows, dist);
            } else if (dist >= 1000) {
                off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "r%d_d=%.1fkm,", roster_rows, dist / 1000.0);
            } else {
                off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "r%d_d=???,", roster_rows);
            }
            if (batt > 0) {
                off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "r%d_b=%d%%,", roster_rows, batt);
            } else {
                off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "r%d_b=-%%,", roster_rows);
            }
            roster_rows++;
        }
    }
    else if (strcmp(current_mode, "RANGE") == 0) {
        off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "range_role:%s,", range_role_sender ? "Sender" : "Receiver");
        off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "range_last_count:%d,", range_last_count);
        off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "range_consecutive_ok:%d,", range_consecutive_ok);
        off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "range_total_loss:%d,", range_total_pckt_loss);
        off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "range_stable_dist:%.1fm,", range_stable_dist);
        off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "range_max_dist:%.1fm,", range_max_dist);
        off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "home_lat:%.6f,", range_home_lat);
        off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "home_lon:%.6f,", range_home_long);
        if (gps_status == GPS_LOC) {
            off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "curr_lat:%.6f,", gps_latitude);
            off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "curr_lon:%.6f,", gps_longitude);
        }
    }
    else if (strcmp(current_mode, "PTT") == 0) {
        off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "ptt_state:%s,", layout_state.ptt_sending ? "TX" : (layout_state.ptt_receiving ? "RX" : "Idle"));
    }
    else if (strcmp(current_mode, "TXT") == 0) {
        uint8_t msg_count = inboxCount();
        off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "txt_inbox_count:%d,", msg_count);
        off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "txt_show_inbox:%d,", txtShowInbox ? 1 : 0);
        off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "txt_scroll_page:%d,", txtInboxScrollPage);
        if (!txtShowInbox && msg_count > 0) {
            uint8_t out_len = 0;
            bool out_trunc = false;
            uint8_t out_sender[4] = {0};
            const char* raw = inboxGet(0, out_len, out_trunc, out_sender);
            if (raw && out_len > 0) {
                static char sanitized[INBOX_MAX_MSG_LEN];
                int si = 0;
                for (uint8_t i = 0; i < out_len && si < sizeof(sanitized) - 1; i++) {
                    if (raw[i] == ',') sanitized[si++] = ':';
                    else sanitized[si++] = raw[i];
                }
                sanitized[si] = '\0';
                off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "txt_latest_msg=%s,", sanitized);
            }
        }
    }
    else if (strcmp(current_mode, "TST") == 0) {
        off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "tst_sent:%d,", test_message_counter);
        off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "tst_recv:%d,", rcv_test_message_counter);
    }
    else if (strcmp(current_mode, "PONG") == 0) {
        off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "pong_state:%d,", layout_state.pong_state);
        off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "pong_rtt_ms:%d,", layout_state.pong_rtt_ms);
    }
    else if (strcmp(current_mode, "SCAN") == 0) {
        off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "s%d@%.0f,", layout_state.scan_progress_pct, currentFrequency);
        for (int i = 0; i < 5 && topChannels[i].frequency > 0; i++) {
            int freq_10k = (int)(topChannels[i].frequency * 10);
            int rssi_10 = (int)(topChannels[i].rssi * 10);
            off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "%08d%d,", freq_10k, rssi_10);
        }
    }
    else if (strcmp(current_mode, "RAW") == 0) {
        off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "raw_count:%d,", pckt_count);
    }
    else if (strcmp(current_mode, "WP") == 0) {
        off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "wp_label=%s,", layout_state.wp_label);
        off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "wp_lat=%.6f,", layout_state.wp_lat);
        off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "wp_lon=%.6f,", layout_state.wp_lon);
        off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "wp_alt=%.1fm,", layout_state.wp_alt);
        off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "wp_broadcasting:%d,", layout_state.wp_broadcasting ? 1 : 0);
        off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "wp_bcast_rem:%ds,", layout_state.wp_bcast_remaining_s);
    }

    // Status bar: freq, time, GPS sats, battery level
    char freq_str[16];
    snprintf(freq_str, sizeof(freq_str), "%.2f", currentFrequency);

    RTC_Date dateTime = rtc.getDateTime();
    char time_str[9];
    snprintf(time_str, sizeof(time_str), "%02d:%02d", dateTime.hour, dateTime.minute);

    uint8_t batt_pct = getBatteryPercentage();
    int bat_idx = 6;
    if (batt_pct > 90) bat_idx = 6;
    else if (batt_pct > 80) bat_idx = 5;
    else if (batt_pct > 60) bat_idx = 4;
    else if (batt_pct > 40) bat_idx = 3;
    else if (batt_pct > 20) bat_idx = 2;
    else if (batt_pct > 10) bat_idx = 1;
    else bat_idx = 0;

    off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "|S:%s|T:%s|G:%d|B:%d%%|I:", freq_str, time_str, gps_satellites, batt_pct);

    if (gps_status == GPS_LOC) {
        off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "gpst=ok,");
    } else {
        off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "gpst=no%,");
    }

    off += snprintf(buf + off, SYNC_MAX_PAYLOAD - off, "bat=%d", bat_idx);
    buf[off] = '\0';

    if (off >= SYNC_MAX_PAYLOAD) {
        sendSerialToAppLn(F("ERR:sendScreenSyncForced overrun"));
        last_batt_idx = saved_batt_idx;
        return;
    }

    sendNotificationToApp(buf);
    
    // Restore tracking state for periodic sync
    last_sync_ms = millis();
    last_batt_idx = saved_batt_idx;
}
