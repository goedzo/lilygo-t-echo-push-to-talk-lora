#include "utilities.h"
#include "display.h"
#include "app_modes.h"
#include "lora.h"
#include <Wire.h>
#include <pcf8563.h>
#include "settings.h"
#include <time.h>  // Include time.h for time manipulation

PCF8563_Class rtc;  // Real-time clock instance
volatile bool rtcInterrupt = false;
void rtcInterruptCb() {
    rtcInterrupt = true;
}

// Global variables defined here and declared as extern in settings.h
char channels[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
int channel_idx = 0;
int volume_level = 5;
int bitrate_idx = 0;
int spreading_factor = 7;  // Default spreading factor SF7

const int bitrate_modes[] = {CODEC2_MODE_3200, CODEC2_MODE_2400, CODEC2_MODE_1600, CODEC2_MODE_1400, CODEC2_MODE_1200, CODEC2_MODE_700};
const size_t num_bitrate_modes = sizeof(bitrate_modes) / sizeof(bitrate_modes[0]);

uint8_t time_setting_mode = 0;  // 0 = hours, 1 = minutes, 2 = seconds
uint8_t setting_idx = 0;        // 0 = bitrate, 1 = volume, 2 = channel, 3 = time, 4 = spreading factor
bool in_settings_mode = false;  // Indicates whether the device is in settings mode

void setupSettings() {
    SerialMon.print("[PCF8563] Initializing ...  ");

    // Set the RTC interrupt pin as input and attach an interrupt handler
    pinMode(RTC_Int_Pin, INPUT);
    attachInterrupt(digitalPinToInterrupt(RTC_Int_Pin), rtcInterruptCb, FALLING);

    // Start I2C communication
    Wire.begin();

    // Retry mechanism to handle potential communication issues
    int retry = 3;
    int ret = 0;
    do {
        // Start communication with the PCF8563 (or HYM8563)
        Wire.beginTransmission(PCF8563_SLAVE_ADDRESS);
        ret = Wire.endTransmission();
        delay(200);  // Delay between retries
    } while (ret != 0 && retry-- > 0);  // Retry until successful or retries exhausted

    // Check if the communication was successful
    if (ret != 0) {
        SerialMon.println("failed");
        return;
    }
    SerialMon.println("success");

    // Initialize the RTC
    rtc.begin(Wire);
    
    // Disable any existing alarms and set an initial time (optional)
    rtc.disableAlarm();
    rtc.setDateTime(2024, 9, 5, 0, 0, 0);  // Set initial time if necessary
}
void toggleSettingsMode() {
    in_settings_mode = !in_settings_mode;
    if (in_settings_mode) {
        setting_idx = 0;  // Start with bitrate
        updDisp(1, "Entered Settings");
    } else {
        updDisp(1, "Exited Settings");
        setupLoRa();
    }
}

void cycleSettings() {
    // Increment the setting index
    setting_idx++;
    
    // Wrap around if it exceeds the number of settings
    if (setting_idx > 4) {  // Adjust based on the number of settings you have
        setting_idx = 0;
    }
    
    // Display the current setting
    displayCurrentSetting();
}

void cycleTimeSettingMode() {
    // Switch between setting hours, minutes, and seconds
    time_setting_mode = (time_setting_mode + 1) % 3;
    displayCurrentTimeSetting();
}

void updateCurrentSetting() {
    // Update the current setting based on which setting is selected
    if (setting_idx == 0) {
        bitrate_idx = (bitrate_idx + 1) % 6;  // Cycle through bitrate options
        updModeAndChannelDisplay();
        updDisp(1, "Bitrate Set");
    } else if (setting_idx == 1) {
        volume_level = (volume_level % 10) + 1;  // Cycle through volume levels (1-10)
        updDisp(1, "Volume Set");
    } else if (setting_idx == 2) {
        updChannel();  // Cycle through channels
    } else if (setting_idx == 3) {
        RTC_Date dateTime = rtc.getDateTime();
        if (time_setting_mode == 0) {
            dateTime.hour = (dateTime.hour + 1) % 24;
        } else if (time_setting_mode == 1) {
            dateTime.minute = (dateTime.minute + 1) % 60;
        } else if (time_setting_mode == 2) {
            dateTime.second = (dateTime.second + 1) % 60;
        }
        rtc.setDateTime(dateTime.year, dateTime.month, dateTime.day, dateTime.hour, dateTime.minute, dateTime.second);
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
    // Get the current time from PCF8563
    RTC_Date dateTime = rtc.getDateTime();
    
    // Display the current time being set
    char time_str[9];  // Format: HH:MM:SS
    snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d", dateTime.hour, dateTime.minute, dateTime.second);

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
        case 0: return 3200;
        case 1: return 2400;
        case 2: return 1600;
        case 3: return 1400;
        case 4: return 1200;
        case 5: return 700;
        default: return 3200;
    }
}
