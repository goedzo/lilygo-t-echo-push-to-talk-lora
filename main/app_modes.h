#ifndef APP_MODES_H
#define APP_MODES_H

#include <AceButton.h>

// Define an array of mode names as strings
extern const char* modes[];
extern const int numModes;
extern int modeIndex;
extern const char* current_mode;


void setupAppModes();  // Initializes the buttons and configures event handling
void handleAppModes();  // Manages the different modes (PTT, TXT, TST, RAW)
void handleEvent(ace_button::AceButton* button, uint8_t eventType, uint8_t buttonState);  // Handles button events
void sendAudio();  // Handles audio transmission in PTT mode
void sendTestMessage();  // Sends a test message in TST mode
void handlePacket();  // Handles received packets
void updMode();  // Cycles through the operation modes
void updChannel();  // Cycles through the channels
void powerOff(); //Powers off the device
void sleepAudio();
void turnoffLed();
#endif