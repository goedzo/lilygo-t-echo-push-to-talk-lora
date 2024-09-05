#ifndef SETTINGS_H
#define SETTINGS_H
#include <codec2.h>
#include <stdint.h>



// Global variables for settings
extern char channels[];
extern int channel_idx;
extern int volume_level;

int bitrate_modes[] = {CODEC2_MODE_3200, CODEC2_MODE_2400, CODEC2_MODE_1600, CODEC2_MODE_1400, CODEC2_MODE_1200, CODEC2_MODE_700};
extern int bitrate_idx;

extern int spreading_factor;

extern uint8_t time_setting_mode;  // 0 = hours, 1 = minutes, 2 = seconds
extern uint8_t setting_idx;        // 0 = bitrate, 1 = volume, 2 = channel, 3 = time, 4 = spreading factor
extern bool in_settings_mode;      // Indicates whether the device is in settings mode


// Enum to represent different operation modes
enum OperationMode { PTT, TXT, TST, RAW };

// Global variable to track the current operation mode
extern OperationMode current_mode;


void setupSettings();
void toggleSettingsMode();
void cycleSettings();  // New function to cycle through settings
void cycleTimeSettingMode();
void updateCurrentSetting();
void displayCurrentSetting();
void displayCurrentTimeSetting();
int getBitrateFromIndex(int index);

#endif // SETTINGS_H