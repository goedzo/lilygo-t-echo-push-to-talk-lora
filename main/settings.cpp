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

// Define the instance of the DeviceSettings struct with default values
DeviceSettings deviceSettings = {
    .bitrate_idx = 2,             // Default to index 2
    .volume_level = 5,            // Default volume level
    .channel_idx = 0,             // Default channel index
    .spreading_factor = 12,        // Default spreading factor (SF9)
    .backlight = true,
    .hours = 0,
    .minutes = 0,
    .seconds = 0
};

// Implementing the methods defined in DeviceSettings struct
void DeviceSettings::nextBitrate() {
    bitrate_idx = (bitrate_idx + 1) % 6;
}

void DeviceSettings::nextVolume() {
    volume_level = (volume_level % 10) + 1;
}

void DeviceSettings::nextChannel() {
    channel_idx = (channel_idx + 1) % 26;  // Assuming 26 channels A-Z
}

void DeviceSettings::incrementTime(int idx, RTC_Date& dateTime) {
    if (idx == HOURS) {
        dateTime.hour = (dateTime.hour + 1) % 24;
    } else if (idx == MINUTES) {
        dateTime.minute = (dateTime.minute + 1) % 60;
    } else if (idx == SECONDS) {
        dateTime.second = (dateTime.second + 1) % 60;
    }
}

void DeviceSettings::nextSpreadingFactor() {
    spreading_factor = spreading_factor == 12 ? 6 : spreading_factor + 1;
}

// Global variables
char channels[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
uint8_t setting_idx = 0;
bool in_settings_mode = false;

const int bitrate_modes[] = {CODEC2_MODE_3200, CODEC2_MODE_2400, CODEC2_MODE_1600, CODEC2_MODE_1400, CODEC2_MODE_1200, CODEC2_MODE_700};
const size_t num_bitrate_modes = sizeof(bitrate_modes) / sizeof(bitrate_modes[0]);

void setupSettings() {
    SerialMon.print("[PCF8563] Initializing ...  ");

    pinMode(RTC_Int_Pin, INPUT);
    attachInterrupt(digitalPinToInterrupt(RTC_Int_Pin), rtcInterruptCb, FALLING);

    Wire.begin();

    int retry = 3, ret = 0;
    do {
        Wire.beginTransmission(PCF8563_SLAVE_ADDRESS);
        ret = Wire.endTransmission();
        delay(200);
    } while (ret != 0 && retry-- > 0);

    if (ret != 0) {
        SerialMon.println("failed");
        return;
    }
    SerialMon.println("success");

    rtc.begin(Wire);
    rtc.disableAlarm();
    rtc.setDateTime(2024, 9, 5, 0, 0, 0);  // Optional initial time setting
}

void toggleSettingsMode() {
    in_settings_mode = !in_settings_mode;
    if (in_settings_mode) {
        clearScreen();
        setting_idx = NUM_SETTINGS;  // Start SPF
        updDisp(1, "Entered Settings",true);
        displayCurrentSetting();
    } else {
        //When leaving settings, the release of the button is still captured, so just move it back one
        modeIndex = modeIndex--;
        if(modeIndex<0) {
            modeIndex = numModes;
        }
        current_mode=modes[modeIndex];

        updDisp(1, "Exited Settings");
        setupLoRa();
        clearScreen();
        updModeAndChannelDisplay();
    }
}

void cycleSettings() {
    setting_idx = (setting_idx + 1) % NUM_SETTINGS;
    displayCurrentSetting();
}

void updateCurrentSetting() {
    RTC_Date dateTime = rtc.getDateTime();

    switch (setting_idx) {
        case SPREADING_FACTOR:
            deviceSettings.nextSpreadingFactor();
            char buf[20];
            snprintf(buf, sizeof(buf), "SF Set to: %d", deviceSettings.spreading_factor);
            updDisp(1, buf, false);
            break;
        case CHANNEL:
            deviceSettings.nextChannel();
            updChannel();
            break;
        case BITRATE:
            deviceSettings.nextBitrate();
            updModeAndChannelDisplay();
            updDisp(1, "Bitrate Set", false);
            break;
        case BACKLIGHT:
            deviceSettings.backlight=!deviceSettings.backlight;
            enableBacklight(deviceSettings.backlight);
            break;
        case VOLUME:
            deviceSettings.nextVolume();
            updDisp(1, "Volume Set", false);
            break;
        case HOURS:
        case MINUTES:
        case SECONDS:
            deviceSettings.incrementTime(setting_idx, dateTime);
            rtc.setDateTime(dateTime.year, dateTime.month, dateTime.day, dateTime.hour, dateTime.minute, dateTime.second);
            displayCurrentTimeSetting();
            break;
    }
    displayCurrentSetting();
}

void displayCurrentSetting() {
    switch (setting_idx) {
        case SPREADING_FACTOR:
            displaySpreadingFactor();
            break;
        case CHANNEL:
            displayChannel();
            break;
        case BITRATE:
            displayBitrate();
            break;
        case BACKLIGHT:
            displayBacklight();
            break;
        case VOLUME:
            displayVolume();
            break;
        case HOURS:
        case MINUTES:
        case SECONDS:
            displayCurrentTimeSetting();
            break;
    }
}

void displayBacklight() {
    updDisp(1, "Backlight:", true);
}


void displayBitrate() {
    updDisp(1, "Bitrate:", false);
    char bitrate_str[20];
    snprintf(bitrate_str, sizeof(bitrate_str), "Bitrate: %d bps", getBitrateFromIndex(deviceSettings.bitrate_idx));
    updDisp(2, bitrate_str);
}

void displayVolume() {
    updDisp(1, "Volume:", false);
    char volume_str[20];
    snprintf(volume_str, sizeof(volume_str), "Volume: %d", deviceSettings.volume_level);
    updDisp(2, volume_str);
}

void displayChannel() {
    updDisp(1, "Channel:", false);
    char channel_str[20];
    snprintf(channel_str, sizeof(channel_str), "Channel: %c", channels[deviceSettings.channel_idx]);
    updDisp(2, channel_str);
}

void displaySpreadingFactor() {
    updDisp(1, "Spreading Factor:", false);
    char sf_str[20];
    snprintf(sf_str, sizeof(sf_str), "SF: %d", deviceSettings.spreading_factor);
    updDisp(2, sf_str);
}

void displayCurrentTimeSetting() {
    RTC_Date dateTime = rtc.getDateTime();
    char time_str[9];  // Format: HH:MM:SS
    snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d", dateTime.hour, dateTime.minute, dateTime.second);

    if (setting_idx == HOURS) {
        updDisp(1, "Setting Hour:");
    } else if (setting_idx == MINUTES) {
        updDisp(1, "Setting Minute:");
    } else if (setting_idx == SECONDS) {
        updDisp(1, "Setting Second:");
    }
    updDisp(2, time_str);
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
