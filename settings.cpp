#include "settings.h"
#include "display.h"
#include "app_modes.h"

void setupSettings() {
    pinMode(MODE_PIN, INPUT_PULLUP);
    pinMode(ACTION_PIN, INPUT_PULLUP);
}

void handleSettings() {
    unsigned long current_time = millis();

    // Long press to enter/exit settings mode
    if (digitalRead(MODE_PIN) == LOW) {
        delay(50);  // Debounce
        unsigned long press_time = millis();
        while (digitalRead(MODE_PIN) == LOW);  // Wait until button is released
        if (millis() - press_time > 1000) {  // Long press detected
            in_settings_mode = !in_settings_mode;
            if (in_settings_mode) {
                setting_idx = 0;  // Start with bitrate
                updDisp(1, "Entered Settings");
            } else {
                updDisp(1, "Exited Settings");
            }
        }
    }
    
    // Settings mode logic
    if (in_settings_mode) {
        if (digitalRead(ACTION_PIN) == LOW) {
            delay(50);  // Debounce
            while (digitalRead(ACTION_PIN) == LOW);  // Wait until button is released
            delay(50);  // Debounce

            if (setting_idx == 0) {
                updMode();  // Change bitrate
            } else if (setting_idx == 1) {
                volume_level = (volume_level % 10) + 1;  // Cycle volume level (example range 1-10)
                updDisp(1, "Volume Set");
            } else if (setting_idx == 2) {
                updChannel();  // Change channel
            }
        }
    } else {
        // Normal operation mode, handle double press or other logic
        if (digitalRead(MODE_PIN) == LOW) {
            delay(50);  // Debounce
            if (waiting_for_second_press) {
                if (current_time - last_button_press_time < 500) {
                    cycleMode();  // Cycle through modes (PTT, TXT, TST, RAW)
                    waiting_for_second_press = false;
                } else {
                    waiting_for_second_press = false;
                }
            } else {
                last_button_press_time = current_time;
                waiting_for_second_press = true;
            }
        } else {
            if (waiting_for_second_press && (current_time - last_button_press_time >= 500)) {
                waiting_for_second_press = false;
            }
        }
    }
}

void cycleMode() {
    if (current_mode == PTT) {
        current_mode = TXT;
        updDisp(1, "Mode: TXT");
    } else if (current_mode == TXT) {
        current_mode = TST;
        updDisp(1, "Mode: TST");
    } else if (current_mode == TST) {
        current_mode = RAW;
        updDisp(1, "Mode: RAW");
    } else if (current_mode == RAW) {
        current_mode = PTT;
        updDisp(1, "Mode: PTT");
    }
}