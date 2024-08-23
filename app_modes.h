#ifndef APP_MODES_H
#define APP_MODES_H

#include <AceButton.h>

// Enum to represent different operation modes
enum OperationMode { PTT, TXT, TST, RAW };

// Global variable to track the current operation mode
extern OperationMode current_mode;

void setupAppModes();  // Initializes the buttons and configures event handling
void handleAppModes();  // Manages the different modes (PTT, TXT, TST, RAW)
void handleEvent(AceButton* button, uint8_t eventType, uint8_t buttonState);  // Handles button events
void sendAudio();  // Handles audio transmission in PTT mode
void sendTestMessage();  // Sends a test message in TST mode
void handlePacket();  // Handles received packets
void updMode();  // Cycles through the operation modes
void updChannel();  // Cycles through the channels
void updModeAndChannelDisplay();  // Updates the display with the current mode and channel

#endif