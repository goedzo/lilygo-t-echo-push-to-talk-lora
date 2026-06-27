#pragma once
#include <bluefruit.h>

void setupBLE();
void handleBLE();
bool isDataPrintable(const uint8_t* data, int length);
void sendNotificationToApp(const char* message);
void sendBinaryNotification(const uint8_t* data, uint8_t len);