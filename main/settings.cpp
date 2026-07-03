#include "utilities.h"
#include "display.h"
#include "app_modes.h"
#include "lora.h"
#include <Wire.h>
#include <pcf8563.h>
#include "settings.h"
#include <time.h>  // Include time.h for time manipulation
#include "display_layout.h"

// PCF8563 I2C address (7-bit)
#define PCF8563_I2C_ADDR    0x51

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
    .spreading_factor = 8,
    .backlight = true,
    .hours = 0,
    .minutes = 0,
    .seconds = 0,
    .bandwidth_idx = BW_250_KHZ, // Set default bandwidth to 250 kHz
    .coding_rate_idx = CR_6,     // Default coding rate 8, to have as much error recovering as possible
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


// Release I2C bus if SDA is held low by a slave (e.g. CST816 touch controller)
// This toggles SCL 10 times to force the slave to release SDA
void releaseI2Cbus() {
    pinMode(SDA_Pin, OUTPUT);
    digitalWrite(SDA_Pin, HIGH);        // Ensure SDA is high
    pinMode(SCL_Pin, OUTPUT);
    for (int i = 0; i < 10; i++) {
        digitalWrite(SCL_Pin, LOW);
        delay(1);
        digitalWrite(SCL_Pin, HIGH);
        delay(1);
    }
    // Restore pins to input (high-Z)
    pinMode(SDA_Pin, INPUT);
    pinMode(SCL_Pin, INPUT);
}

void setupSettings() {
    sendSerialToAppLn("[SETTINGS] >>> setupSettings() START");
    
    pinMode(RTC_Int_Pin, INPUT);
    attachInterrupt(digitalPinToInterrupt(RTC_Int_Pin), rtcInterruptCb, FALLING);
    sendSerialToAppLn("[SETTINGS]   RTC interrupt configured");

    // On nRF52 T-Echo, the CST816 touch controller shares I2C with PCF8563.
    // After power-on, CST816 may hold SDA low causing Wire.beginTransmission to hang forever.
    // Workaround: release I2C bus before attempting any I2C communication.
    
    sendSerialToAppLn("[SETTINGS]   Releasing I2C bus from touch controller...");
    releaseI2Cbus();

    sendSerialToAppLn("[SETTINGS]   Using Wire probe for RTC");

    Wire.begin();

    int retry = 5;
    uint8_t txAddr = PCF8563_I2C_ADDR;  // 7-bit address, Wire.beginTransmission handles shifting
    
    int ret = 0;
    unsigned long start = millis();
    do {
        sendSerialToAppLn("[SETTINGS]   RTC probe attempt " + String(6 - retry) + "/5...");
        Wire.beginTransmission(txAddr);
        ret = Wire.endTransmission();
        if (ret == 0) break;  // Success
        delay(200);
    } while (retry--);
    
    unsigned long probeTime = millis() - start;

    if (ret != 0) {
        sendSerialToAppLn("[SETTINGS]   RTC not responding on I2C (probed in " + String(probeTime) + "ms)");
        
        // Still init Wire for other uses (touch controller etc.)
        Wire.begin();
        sendSerialToAppLn("[SETTINGS]   Wire begin OK (no RTC)");
    } else {
        sendSerialToAppLn("[SETTINGS]   Wire probe: PCF8563 FOUND (probed in " + String(probeTime) + "ms)");
        rtc.begin(Wire);
        rtc.disableAlarm();
        
        RTC_Date dt = rtc.getDateTime();
        char timeBuf[32];
        snprintf(timeBuf, sizeof(timeBuf), "RTC: %04d-%02d-%02d %02d:%02d:%02d", 
                 dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);
        sendSerialToAppLn("[SETTINGS]   RTC: " + String(timeBuf));
    }

    sendSerialToAppLn("[SETTINGS] <<< setupSettings() DONE");
}

void toggleSettingsMode() {
    in_settings_mode = !in_settings_mode;
    if (in_settings_mode) {
        forceFullRefresh();
        drawDefaultLayout();
        forceFullRefresh();
        displayCurrentSetting();
    } else {
        if (radio) {
            setupLoRa();
        }
        forceFullRefresh();
        updModeAndChannelDisplay();
    }
}

void cycleSettings() {
    setting_idx = (setting_idx + 1) % NUM_SETTINGS;
    drawSettingsLayout();
}

void updateCurrentSetting() {
    RTC_Date dateTime = rtc.getDateTime();

    switch (setting_idx) {
        case SPREADING_FACTOR:
            deviceSettings.nextSpreadingFactor();
            break;
        case CHANNEL:
            deviceSettings.nextChannel();
            break;
        case BITRATE:
            deviceSettings.nextBitrate();
            break;
        case BACKLIGHT:
            deviceSettings.backlight=!deviceSettings.backlight;
            enableBacklight(deviceSettings.backlight);
            break;
        case VOLUME:
            deviceSettings.nextVolume();
            break;
        case HOURS:
        case MINUTES:
        case SECONDS:
            deviceSettings.incrementTime(setting_idx, dateTime);
            rtc.setDateTime(dateTime.year, dateTime.month, dateTime.day, dateTime.hour, dateTime.minute, dateTime.second);
            break;
        case BANDWIDTH:
            deviceSettings.nextBandwidth();
            break;
        case CODING_RATE:
            deviceSettings.nextCodingRate();
            break;
        case FREQUENCY_HOPPING:
            deviceSettings.toggleFrequencyHopping();
            break;
    }
    drawSettingsLayout();
}

void displayCurrentSetting() {
    drawSettingsLayout();
}

// Legacy functions kept for backward compatibility — no-ops since rendering uses drawSettingsLayout()

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
