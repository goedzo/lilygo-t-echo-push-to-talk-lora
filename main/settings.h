#ifndef SETTINGS_H
#define SETTINGS_H

#include <codec2.h>
#include <stdint.h>
#include <cstddef>
#include <pcf8563.h>

// Buffer sizes and other constants
#define RAW_SIZE 160  // Adjust as necessary
#define MAX_PKT 255   // Maximum packet size

// Enum for settings note that the first must be =0!
enum Setting {
    SPREADING_FACTOR=0,
    CHANNEL,
    BITRATE,
    BACKLIGHT,
    VOLUME,
    HOURS,
    MINUTES,
    SECONDS,
    BANDWIDTH,    // New bandwidth setting
    CODING_RATE,  // New coding rate setting
    NUM_SETTINGS
};

// Enum for Bandwidth with actual values
enum Bandwidth {
    BW_7_8_KHZ = 7800,
    BW_10_4_KHZ = 10400,
    BW_15_6_KHZ = 15600,
    BW_20_8_KHZ = 20800,
    BW_31_25_KHZ = 31250,
    BW_41_7_KHZ = 41700,
    BW_62_5_KHZ = 62500,
    BW_125_KHZ = 125000,
    BW_250_KHZ = 250000,
    BW_500_KHZ = 500000
};

// Enum for CodingRate with actual values
enum CodingRate {
    CR_5 = 5,
    CR_6 = 6,
    CR_7 = 7,
    CR_8 = 8
};

// Struct for device settings
struct DeviceSettings {
    int bitrate_idx;
    int volume_level;
    int channel_idx;
    int spreading_factor;
    bool backlight;

    // Time-related settings
    int hours;
    int minutes;
    int seconds;

    // New settings
    int bandwidth_idx;   // Index for bandwidth settings
    int coding_rate_idx; // Index for coding rate settings

    // Methods to increment or cycle settings
    void nextBitrate();
    void nextVolume();
    void nextChannel();
    void incrementTime(int idx, RTC_Date& dateTime);
    void nextSpreadingFactor();
    void nextBandwidth();   // New method for bandwidth
    void nextCodingRate();  // New method for coding rate
};

// External declarations for global variables
extern DeviceSettings deviceSettings;  // Instance of DeviceSettings struct
extern char channels[];                // List of channels (e.g., A-Z)
extern const int bitrate_modes[];      // Array of bitrate modes
extern const size_t num_bitrate_modes; // Number of bitrate modes

extern PCF8563_Class rtc;              // Allow the RTC to be set from the GPS

extern uint8_t setting_idx;  // Index for the current setting (using the Setting enum)
extern bool in_settings_mode; // Flag indicating whether the device is in settings mode
extern bool time_set;

// Function prototypes
void setupSettings();
void toggleSettingsMode();
void cycleSettings();
void updateCurrentSetting();
void displayCurrentSetting();
void displayCurrentTimeSetting();
void displayBitrate();
void displayVolume();
void displayChannel();
void displaySpreadingFactor();
void displayBacklight();
void displayBandwidth();   // New function to display bandwidth
void displayCodingRate();  // New function to display coding rate
int getBitrateFromIndex(int index);

#endif // SETTINGS_H
