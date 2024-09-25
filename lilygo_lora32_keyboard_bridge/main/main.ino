#include "display.h"
#include "bluetooth.h"
#include "ble.h"
#include "globals.h"

// Define the constants and variables
const int MAX_TRIED_DEVICES = 50;
String triedDevices[MAX_TRIED_DEVICES];
int triedDeviceCount = 0;

void setup() {
  Serial.begin(115200);
  initDisplay();
  initBluetooth();
  initBLE();
  scanClassicBT();
}

void loop() {
  // Handle Bluetooth communication and display updates
  if (SerialBT.connected() && SerialBT.available()) {
    char incomingChar = SerialBT.read();
    Serial.printf("Received key: %c\n", incomingChar);
    sendKeyToBLE(incomingChar);
    displayInfo("", "", "", "", &incomingChar);
  }
}
