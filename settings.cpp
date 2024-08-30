#include "settings.h"
#include "display.h"
#include "app_modes.h"
#include <RTCZero.h>

RTCZero rtc;  // Real-time clock instance

// Global variables defined here and declared as extern in settings.h
char channels[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
int channel_idx = 0;
int volume_level = 5;
int bitrate_idx = 0;
int spreading_factor = 7;  // Default spreading factor SF7


uint8_t time_setting_mode = 0;  // 0 = hours, 1 = minutes, 2 = seconds
uint8_t setting_idx = 0;        // 0 = bitrate, 1 = volume, 2 = channel, 3 = time
bool in_settings_mode = false;  // Indicates whether the device is in settings mode

void setupSettings() {
    // Initialize the RTC
    rtc.begin();
}

void toggleSettingsMode() {
    in_settings_mode = !in_settings_mode;
    if (in_settings_mode) {
        setting_idx = 0;  // Start with bitrate
        updDisp(1, "Entered Settings");
    } else {
        updDisp(1, "Exited Settings");
    }
}

void cycleTimeSettingMode() {
    // Switch between setting hours, minutes, and seconds
    time_setting_mode = (time_setting_mode + 1) % 3;
    displayCurrentTimeSetting();
}

void updateCurrentSetting() {
    // Update the current setting based on which setting is selected
    if (setting_idx == 0) {
        bitrate_idx = (bitrate_idx + 1) % 4;  // Cycle through bitrate options
        updModeAndChannelDisplay();
        updDisp(1, "Bitrate Set");
    } else if (setting_idx == 1) {
        volume_level = (volume_level % 10) + 1;  // Cycle through volume levels (1-10)
        updDisp(1, "Volume Set");
    } else if (setting_idx == 2) {
        updChannel();  // Cycle through channels
    } else if (setting_idx == 3) {
        if (time_setting_mode == 0) {
            int hour = rtc.getHours();
            rtc.setHours((hour + 1) % 24);
        } else if (time_setting_mode == 1) {
            int minute = rtc.getMinutes();
            rtc.setMinutes((minute + 1) % 60);
        } else if (time_setting_mode == 2) {
            int second = rtc.getSeconds();
            rtc.setSeconds((second + 1) % 60);
        }
        displayCurrentTimeSetting();
    } else if (setting_idx == 4) {
        spreading_factor = spreading_factor == 12 ? 6 : spreading_factor + 1; // Cycle between SF6 and SF12
        char buf[20];
        snprintf(buf, sizeof(buf), "SF Set to: %d", spreading_factor);
        updDisp(1, buf);
    }
}

void displayCurrentSetting() {
    // Display the current setting being adjusted
    if (setting_idx == 0) {
        updDisp(1, "Setting Bitrate:");
        char bitrate_str[20];
        snprintf(bitrate_str, sizeof(bitrate_str), "Bitrate: %d bps", getBitrateFromIndex(bitrate_idx));
        updDisp(2, bitrate_str);
    } else if (setting_idx == 1) {
        updDisp(1, "Setting Volume:");
        char volume_str[20];
        snprintf(volume_str, sizeof(volume_str), "Volume: %d", volume_level);
        updDisp(2, volume_str);
    } else if (setting_idx == 2) {
        updDisp(1, "Setting Channel:");
        char channel_str[20];
        snprintf(channel_str, sizeof(channel_str), "Channel: %c", channels[channel_idx]);
        updDisp(2, channel_str);
    } else if (setting_idx == 3) {
        displayCurrentTimeSetting();  // Reuse the function to display current time setting
    } else if (setting_idx == 4) {
        updDisp(1, "Setting Spreading Factor:");
        char sf_str[20];
        snprintf(sf_str, sizeof(sf_str), "SF: %d", spreading_factor);
        updDisp(2, sf_str);
    }
}

void displayCurrentTimeSetting() {
    // Display the current time being set
    char time_str[9];  // Format: HH:MM:SS
    snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d", rtc.getHours(), rtc.getMinutes(), rtc.getSeconds());

    if (time_setting_mode == 0) {
        updDisp(1, "Setting Hour:");
    } else if (time_setting_mode == 1) {
        updDisp(1, "Setting Minute:");
    } else if (time_setting_mode == 2) {
        updDisp(1, "Setting Second:");
    }
    updDisp(2, time_str);  // Display the current time value
}

// Example function to map the bitrate index to actual bitrate value (bps)
int getBitrateFromIndex(int index) {
    switch (index) {
        case 0: return 2400;
        case 1: return 4800;
        case 2: return 9600;
        case 3: return 19200;
        default: return 2400;
    }
}