#include "settings.h"
#include "display.h"
#include "app_modes.h"
#include <RTCZero.h>
#include <AceButton.h>

using namespace ace_button;

// RTC object
RTCZero rtc;

// Button objects
AceButton modeButton(MODE_PIN);
AceButton actionButton(ACTION_PIN);

uint8_t time_setting_mode = 0;  // 0 = hours, 1 = minutes, 2 = seconds
uint8_t setting_idx = 0;        // 0 = bitrate, 1 = volume, 2 = channel, 3 = time
bool in_settings_mode = false;

// Volume, Channel, and Bitrate settings
int volume_level = 5;            // Example range from 1 to 10
int channel_idx = 0;             // Index for current channel
char channels[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
int bitrate_idx = 0;             // Example bitrate index (this can map to specific bitrate values)

void handleEvent(AceButton*, uint8_t, uint8_t);

void setupSettings() {
    pinMode(MODE_PIN, INPUT_PULLUP);
    pinMode(ACTION_PIN, INPUT_PULLUP);

    // Initialize the RTC
    rtc.begin();

    // Initialize buttons
    ButtonConfig* config = ButtonConfig::getSystemButtonConfig();
    config->setEventHandler(handleEvent);

    modeButton.init(MODE_PIN);
    actionButton.init(ACTION_PIN);
}

void handleEvent(AceButton* button, uint8_t eventType, uint8_t buttonState) {
    if (button->getPin() == MODE_PIN) {
        if (eventType == AceButton::kEventLongPressed) {
            in_settings_mode = !in_settings_mode;
            if (in_settings_mode) {
                setting_idx = 0;  // Start with bitrate
                updDisp(1, "Entered Settings");
            } else {
                updDisp(1, "Exited Settings");
            }
        } else if (eventType == AceButton::kEventPressed && in_settings_mode) {
            // Cycle through different settings (bitrate, volume, channel, time)
            setting_idx = (setting_idx + 1) % 4;
            displayCurrentSetting();
        }
    } else if (button->getPin() == ACTION_PIN) {
        if (in_settings_mode) {
            if (eventType == AceButton::kEventLongPressed && setting_idx == 3) {
                // Switch between hours, minutes, and seconds in time setting
                time_setting_mode = (time_setting_mode + 1) % 3;
                displayCurrentTimeSetting();
            } else if (eventType == AceButton::kEventPressed) {
                // Increment the current setting
                if (setting_idx == 0) {
                    // Cycle bitrate index (example range from 0 to 3)
                    bitrate_idx = (bitrate_idx + 1) % 4;
                    // Update the bitrate display
                    updMode();
                    updDisp(1, "Bitrate Set");
                } else if (setting_idx == 1) {
                    // Cycle volume level (example range from 1 to 10)
                    volume_level = (volume_level % 10) + 1;
                    updDisp(1, "Volume Set");
                } else if (setting_idx == 2) {
                    // Cycle through channels
                    channel_idx = (channel_idx + 1) % 26;  // Assuming 26 channels A-Z
                    updChannel();
                } else if (setting_idx == 3) {
                    // Adjust the time
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
                }
            }
        }
    }
}

void displayCurrentSetting() {
    if (setting_idx == 0) {
        updDisp(1, "Setting Bitrate:");
        // Display current bitrate (assuming you have an array or function to get the bitrate value from the index)
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
    }
}

void displayCurrentTimeSetting() {
    char time_str[9];  // HH:MM:SS
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