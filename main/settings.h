#ifndef SETTINGS_H
#define SETTINGS_H
#include <codec2.h>
#include <stdint.h>
#include <cstddef>

// Global variables for settings
extern char channels[];
extern int channel_idx;
extern int volume_level;
extern int bitrate_idx;

extern const int bitrate_modes[];
extern const size_t num_bitrate_modes;

extern int spreading_factor;

extern uint8_t setting_idx;        // 0 = bitrate, 1 = volume, 2 = channel, 3 = hours, 4 = minutes, 5 = seconds, 6 = spreading factor
extern bool in_settings_mode;      // Indicates whether the device is in settings mode

void setupSettings();
void toggleSettingsMode();
void cycleSettings();  // Updated function to cycle through settings
void updateCurrentSetting();
void displayCurrentSetting();
void displayCurrentTimeSetting();
int getBitrateFromIndex(int index);

#endif // SETTINGS_H
