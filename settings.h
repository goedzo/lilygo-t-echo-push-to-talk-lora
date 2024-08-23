#ifndef SETTINGS_H
#define SETTINGS_H

// Global variables for settings
extern char channels[];
extern int channel_idx;
extern int volume_level;
extern int bitrate_idx;

extern uint8_t time_setting_mode;  // 0 = hours, 1 = minutes, 2 = seconds
extern uint8_t setting_idx;        // 0 = bitrate, 1 = volume, 2 = channel, 3 = time
extern bool in_settings_mode;      // Indicates whether the device is in settings mode

void setupSettings();
void toggleSettingsMode();
void cycleTimeSettingMode();
void updateCurrentSetting();
void displayCurrentSetting();
void displayCurrentTimeSetting();
int getBitrateFromIndex(int index);

#endif