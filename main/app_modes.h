#ifndef APP_MODES_H
#define APP_MODES_H

#include <AceButton.h>
#include "packet.h"

// Define an array of mode names as strings
extern const char* modes[];
extern const int numModes;
extern int modeIndex;
extern const char* current_mode;
extern double range_home_lat;
extern double range_home_long;

void sendTxtMessage(const char* message);

// Waypoint storage in RTC RAM (same region as text inbox)
#define WP_RTC_START 0x200075C0
#define WP_MAX_ENTRIES 8
#define WP_MAX_LABEL 24
struct WaypointEntry {
    double lat;
    double lon;
    float  alt;
    char   label[WP_MAX_LABEL + 1];
    String senderId;
    uint32_t timestamp; // RTC seconds stored in RTC RAM
    bool valid;
};
extern WaypointEntry wpStoredWaypoints[WP_MAX_ENTRIES];
extern int     wpStoredCount;

// Waypoint broadcast
extern bool wpBroadcastActive;
extern uint32_t wpBroadcastStartMs;

void startWaypointBroadcast(double lat, double lon, float alt, const char* label);

// Core mode functions
void setupAppModes();
void handleAppModes();
void handleEvent(ace_button::AceButton* button, uint8_t eventType, uint8_t buttonState);
void sendRangeMessage();
bool debouncedTouchPress();
void sendAudio();
void sendTestMessage(bool now=false);
void handlePacket(Packet packet);
void updMode();
void updChannel();
void powerOff();
void sleepAudio();
void turnoffLed();
void printRangeStatus();
void switchMode(String receivedMode);

// Text inbox display state (TXT mode)
extern uint8_t  txtInboxScrollPage;
extern uint8_t  txtInboxMsgCount;
extern bool     txtShowInbox;       // true = showing inbox, false = showing single latest
void txtModeInboxDisplay();
void txtModeToggleInboxView();
void txtModeClearInbox();

#endif