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
    NUM_SETTINGS  // Total number of settings
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


    // Methods to increment or cycle settings
    void nextBitrate();
    void nextVolume();
    void nextChannel();
    void incrementTime(int idx, RTC_Date& dateTime);
    void nextSpreadingFactor();
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
int getBitrateFromIndex(int index);

#endif // SETTINGS_H
