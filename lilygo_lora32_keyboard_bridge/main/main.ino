#include <BluetoothSerial.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLEHIDDevice.h>
#include <HIDTypes.h>
#include <HIDKeyboardTypes.h>  // From ESP32 BLE HID library
#include <U8g2lib.h>

// Initialize your display (adjust as per your display type)
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE); 

// Display function to update screen
void displayInfo(const String& deviceName, const String& deviceAddr, const char* incomingChar = nullptr) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x8_tr);  // Choose a font
  u8g2.setCursor(0, 15);
  
  if (!deviceName.isEmpty() && !deviceAddr.isEmpty()) {
    u8g2.print("Device: ");
    u8g2.print(deviceName);
    u8g2.setCursor(0, 30);
    u8g2.print("Address: ");
    u8g2.print(deviceAddr);
  } else if (incomingChar) {
    u8g2.print("Received Key: ");
    u8g2.print(incomingChar);
  } else {
    u8g2.print("Scanning for devices...");
  }
  
  u8g2.sendBuffer();  // Transfer internal memory to the display
}

BluetoothSerial SerialBT;  // Classic Bluetooth object for scanning and connections
BLEHIDDevice* hid;         // BLE HID Device for relaying Classic Bluetooth input as BLE
BLEServer* pServer;        // BLE Server to manage BLE connections

#define MAX_TRIED_DEVICES 50
String triedDevices[MAX_TRIED_DEVICES];  // Store tried device addresses
int triedDeviceCount = 0;

uint32_t lastConnectionAttempt = 0;
uint32_t heartbeat = 0;
bool isConnecting = false;

#define CONNECTION_TIMEOUT_MS 5000
#define HEARTBEAT_TIMEOUT_MS 5000

// Helper function to convert a MAC address to a string
String addressToString(uint8_t* addr) {
  char str[18];  // MAC address as string "XX:XX:XX:XX:XX:XX"
  snprintf(str, sizeof(str), "%02X:%02X:%02X:%02X:%02X:%02X", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
  return String(str);
}

bool isDeviceInTriedList(String addr) {
  for (int i = 0; i < triedDeviceCount; i++) {
    if (triedDevices[i] == addr) {
      return true;  // Device already tried
    }
  }
  return false;
}

void addDeviceToTriedList(String addr) {
  if (triedDeviceCount < MAX_TRIED_DEVICES) {
    triedDevices[triedDeviceCount] = addr;  // Add the address as a string
    triedDeviceCount++;
  }
}

// Scanning for Classic Bluetooth devices
void scanClassicBT() {
  if (!SerialBT.hasClient()) {
    Serial.println("Scanning for Classic Bluetooth devices...");
    // Connect to a known address or let the user choose from found devices
    displayInfo("", "");  // Show scanning message
   }
}


// Classic Bluetooth device callback to filter for HID devices like keyboards
bool callbackBTDevice(String btDeviceAddr, String btDeviceName) {
  if (isDeviceInTriedList(btDeviceAddr)) {
    Serial.println("Skipping tried device.");
    return true;  // Continue scanning
  }

  // Add device to tried list
  addDeviceToTriedList(btDeviceAddr);

  // Check if the device name indicates it might be a keyboard
  if (btDeviceName.indexOf("Keyboard") != -1) {
    Serial.println("Classic Bluetooth Keyboard detected! Connecting...");
    SerialBT.connect(btDeviceAddr.c_str());
    displayInfo(btDeviceName, btDeviceAddr);  // Display detected keyboard info
    return false;  // Stop scanning to connect
  }

  Serial.printf("New Device: %s (%s)\n", btDeviceName.c_str(), btDeviceAddr.c_str());
  return true;  // Continue scanning
}

void setup() {
  Serial.begin(115200);
  
  // Initialize display
  u8g2.begin();
  
  // Initialize Classic Bluetooth
  if (!SerialBT.begin("ESP32_BT_Classic")) {
    Serial.println("An error occurred initializing Classic Bluetooth");
    return;
  }

  Serial.println("Classic Bluetooth started. Scanning for keyboards...");
  displayInfo("", "");  // Initial display message

  // Initialize BLE HID for keyboard emulation
  BLEDevice::init("ESP32_BLE_Keyboard");
  pServer = BLEDevice::createServer();
  hid = new BLEHIDDevice(pServer);

  hid->manufacturer()->setValue("ESP32 Classic-to-BLE");
  hid->pnp(0x01, 0x02E5, 0xABCD, 0x0110);
  hid->hidInfo(0x00, 0x01);

  // Start HID services
  hid->startServices();
  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->start();
  Serial.println("BLE HID started.");

  // Scan for Classic Bluetooth devices
  scanClassicBT();
}

void loop() {
  // Heartbeat to show system is running
  if (millis() - heartbeat > HEARTBEAT_TIMEOUT_MS) {
    Serial.print(".");
    heartbeat = millis();
  }

  // If connected to a Classic Bluetooth keyboard, read data and send as BLE HID
  if (SerialBT.connected()) {
    if (SerialBT.available()) {
      char incomingChar = SerialBT.read();
      Serial.printf("Received key: %c\n", incomingChar);

      // Convert the character to HID keycode and send it via BLE
      uint8_t key[] = {0, 0, charToHID(incomingChar), 0, 0, 0, 0, 0};
      hid->inputReport(1)->setValue(key, sizeof(key));
      hid->inputReport(1)->notify();

      // Display received key on the screen
      displayInfo("", "", &incomingChar);

      // Release the key
      delay(100);
      uint8_t keyRelease[] = {0, 0, 0, 0, 0, 0, 0, 0};
      hid->inputReport(1)->setValue(keyRelease, sizeof(keyRelease));
      hid->inputReport(1)->notify();
    }
  }
}


// Helper function to map ASCII characters to HID keycodes
uint8_t charToHID(char key) {
  // For lowercase letters
  if (key >= 'a' && key <= 'z') {
    return keymap[key - 'a' + 0x04].usage;  // Usage values for 'a' to 'z'
  }
  
  // For uppercase letters
  if (key >= 'A' && key <= 'Z') {
    return keymap[key - 'A' + 0x04].usage;  // Usage values for 'A' to 'Z'
  }

  // For digits 0-9
  if (key >= '0' && key <= '9') {
    return keymap[key - '0' + 0x27].usage;  // Usage values for '0' to '9'
  }

  // For special characters (space, enter, etc.)
  switch (key) {
    case ' ':
      return 0x2C;  // Space bar
    case '\n':
      return 0x28;  // Enter key
    default:
      return 0;  // For unhandled characters
  }
}
