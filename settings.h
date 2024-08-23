#ifndef SETTINGS_H
#define SETTINGS_H

void setupSettings();
void handleEvent(AceButton* button, uint8_t eventType, uint8_t buttonState);
void displayCurrentSetting();
void displayCurrentTimeSetting();
int getBitrateFromIndex(int index);

#endif