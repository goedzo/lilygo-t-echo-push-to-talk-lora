#include "utilities.h"
#include "display.h"
#include "app_modes.h"
#include "lora.h"
#include <Wire.h>
#include <pcf8563.h>
#include "settings.h"
#include <time.h>  // Include time.h for time manipulation

unsigned long sharedSeed = 7529;
PCF8563_Class rtc;  // Real-time clock instance
bool time_set = false;
volatile bool rtcInterrupt = false;
void rtcInterruptCb() {
    rtcInterrupt = true;
}

// Initialize device settings with default values
// Initialize device settings with default values
DeviceSettings deviceSettings = {
    .bitrate_idx = 2,
    .volume_level = 5,
    .channel_idx = 0,
    .spreading_factor = 9,
    .backlight = true,
    .hours = 0,
    .minutes = 0,
    .seconds = 0,
    .bandwidth_idx = BW_62_5_KHZ, // Set default bandwidth to 250 kHz
    .coding_rate_idx = CR_7,     // Default coding rate 8, to have as much error recovering as possible
    .frequency_hopping_enabled = true  // Enable frequency hopping by default
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
    time_set = true;
    if (idx == HOURS) {
        dateTime.hour = (dateTime.hour + 1) % 24;
    } else if (idx == MINUTES) {
        dateTime.minute = (dateTime.minute + 1) % 60;
    } else if (idx == SECONDS) {
        dateTime.second = (dateTime.second + 1) % 60;
    }
}

void DeviceSettings::nextSpreadingFactor() {
    //spreading_factor = spreading_factor == 6 ? 12 : spreading_factor - 1;
    spreading_factor = spreading_factor == 12 ? 6 : spreading_factor + 1;
}

void DeviceSettings::nextBandwidth() {
    if (bandwidth_idx == BW_500_KHZ) {
        bandwidth_idx = BW_7_8_KHZ;  // Cycle back to the smallest value
    } else {
        switch (bandwidth_idx) {
            case BW_7_8_KHZ: bandwidth_idx = BW_10_4_KHZ; break;
            case BW_10_4_KHZ: bandwidth_idx = BW_15_6_KHZ; break;
            case BW_15_6_KHZ: bandwidth_idx = BW_20_8_KHZ; break;
            case BW_20_8_KHZ: bandwidth_idx = BW_31_25_KHZ; break;
            case BW_31_25_KHZ: bandwidth_idx = BW_41_7_KHZ; break;
            case BW_41_7_KHZ: bandwidth_idx = BW_62_5_KHZ; break;
            case BW_62_5_KHZ: bandwidth_idx = BW_125_KHZ; break;
            case BW_125_KHZ: bandwidth_idx = BW_250_KHZ; break;
            case BW_250_KHZ: bandwidth_idx = BW_500_KHZ; break;
        }
    }
}

void DeviceSettings::nextCodingRate() {
    if (coding_rate_idx == CR_8) {
        coding_rate_idx = CR_5;  // Cycle back to the smallest value
    } else {
        switch (coding_rate_idx) {
            case CR_5: coding_rate_idx = CR_6; break;
            case CR_6: coding_rate_idx = CR_7; break;
            case CR_7: coding_rate_idx = CR_8; break;
        }
    }
}

// Method to toggle frequency hopping
void DeviceSettings::toggleFrequencyHopping() {
    frequency_hopping_enabled = !frequency_hopping_enabled;
    if(!frequency_hopping_enabled) {
        setFrequency(defaultFrequency);
    }
}

// Global variables
char channels[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
uint8_t setting_idx = 0;
bool in_settings_mode = false;

const int bitrate_modes[] = {CODEC2_MODE_3200, CODEC2_MODE_2400, CODEC2_MODE_1600, CODEC2_MODE_1400, CODEC2_MODE_1200, CODEC2_MODE_700};
const size_t num_bitrate_modes = sizeof(bitrate_modes) / sizeof(bitrate_modes[0]);

void setupSettings() {
    Serial.print("setupSettings Initializing ...  ");

    //We need to make sure the I2C bus is properly initialized

    pinMode(SCL_Pin, OUTPUT);
    for (int i = 0; i < 9; i++) {
        digitalWrite(SCL_Pin, HIGH);
        delay(10);
        digitalWrite(SCL_Pin, LOW);
        delay(10);
    }
    Wire.begin();  // Re-initialize I2C bus    

    Serial.print("pinMode(RTC_Int_Pin, INPUT);");
    pinMode(RTC_Int_Pin, INPUT);
    Serial.print("attachInterrupt(digitalPinToInterrupt(RTC_Int_Pin), rtcInterruptCb, FALLING);");
    attachInterrupt(digitalPinToInterrupt(RTC_Int_Pin), rtcInterruptCb, FALLING);

    Serial.println("Starting RTC");
    Wire.begin();

    int retry = 3, ret = 0;
    do {
        Serial.println("Wire.beginTransmission(PCF8563_SLAVE_ADDRESS);");
        Wire.beginTransmission(PCF8563_SLAVE_ADDRESS);
        delay(200);
        ret = Wire.endTransmission();
        Serial.println("Wire.endTransmission");
        Serial.println(ret);
    } while (ret != 0 && retry-- > 0);

    if (ret != 0) {
        Serial.println("failed");
        return;
    }
    Serial.println("success");
    rtc.begin(Wire);
    rtc.disableAlarm();
    //rtc.setDateTime(2024, 9, 5, 0, 0, 0);  // Optional initial time setting
}

void toggleSettingsMode() {
    in_settings_mode = !in_settings_mode;
    if (in_settings_mode) {
        clearScreen();
        updDisp(1, "Entered Settings",true);
        displayCurrentSetting();
    } else {
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
        case BANDWIDTH:
            deviceSettings.nextBandwidth();
            displayBandwidth();
            break;
        case CODING_RATE:
            deviceSettings.nextCodingRate();
            displayCodingRate();
            break;
        case FREQUENCY_HOPPING:   // New case for frequency hopping
            deviceSettings.toggleFrequencyHopping();
            displayFrequencyHopping();
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
        case BANDWIDTH:
            displayBandwidth();
            break;
        case CODING_RATE:
            displayCodingRate();
            break;
        case FREQUENCY_HOPPING:   // New display case for frequency hopping
            displayFrequencyHopping();
            break;
    }
}

void displayBacklight() {
    updDisp(1, "Backlight:", true);
    if(deviceSettings.backlight) {
      updDisp(2, "On", true);
    }
    else {
      updDisp(2, "Off", true);
    }
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

void displayBandwidth() {
    updDisp(1, "Bandwidth:", false);
    
    // Convert bandwidth from Hz to kHz (float value)
    float bandwidth_in_khz = deviceSettings.bandwidth_idx / 1000.0;
    
    // Create a string to display the current bandwidth
    char bw_str[20];
    snprintf(bw_str, sizeof(bw_str), "BW: %.2f kHz", bandwidth_in_khz);
    
    // Update display with the current bandwidth
    updDisp(2, bw_str);
}

void displayCodingRate() {
    updDisp(1, "Coding Rate:", false);
    char cr_str[20];
    snprintf(cr_str, sizeof(cr_str), "CR: %d", deviceSettings.coding_rate_idx);
    updDisp(2, cr_str);
}

void displayFrequencyHopping() {
    updDisp(1, "Freqncy Hopping:", true);
    if (deviceSettings.frequency_hopping_enabled) {
        updDisp(2, "Enabled", true);
    } else {
        updDisp(2, "Disabled", true);
    }
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
