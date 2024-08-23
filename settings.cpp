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

void handleSettings() {
    // Update buttons
    modeButton.check();
    actionButton.check();

    // Regularly update the time display
    displayCurrentTimeSetting();
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
        }
    } else if (button->getPin() == ACTION_PIN) {
        if (in_settings_mode) {
            if (eventType == AceButton::kEventLongPressed) {
                // Switch between hours, minutes, and seconds
                time_setting_mode = (time_setting_mode + 1) % 3;
                displayCurrentTimeSetting();
            } else if (eventType == AceButton::kEventPressed) {
                // Increment the current time setting
                if (setting_idx == 0) {
                    updMode();  // Change bitrate
                } else if (setting_idx == 1) {
                    volume_level = (volume_level % 10) + 1;  // Cycle volume level (example range 1-10)
                    updDisp(1, "Volume Set");
                } else if (setting_idx == 2) {
                    updChannel();  // Change channel
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
                }
            }
        }
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