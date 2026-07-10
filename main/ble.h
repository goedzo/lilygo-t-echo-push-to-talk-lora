#pragma once
#include <bluefruit.h>

extern uint32_t ble_connect_stall_until;
extern volatile bool cccd_subscribed;  // True after central subscribes to notify characteristic

void setupBLE();
void handleBLE();
bool isDataPrintable(const uint8_t* data, int length);
void sendNotificationToApp(const char* message);
bool sendFragmentedNotification(const char* message);
void sendBinaryNotification(const uint8_t* data, uint8_t len);
bool isPhoneConnected();
bool isBleConnected();
void serialHookInit();
int getPendingNotificationCount();