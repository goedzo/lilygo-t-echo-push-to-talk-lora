#include <BluetoothSerial.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLEHIDDevice.h>
#include <HIDTypes.h>
#include <HIDKeyboardTypes.h>  // From ESP32 BLE HID library
#include <U8g2lib.h>
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"

// Initialize your display (adjust as per your display type)
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);

// Bluetooth Serial object for Classic Bluetooth communication
BluetoothSerial SerialBT;  // Classic Bluetooth object for scanning and connections
BLEHIDDevice* hid;         // BLE HID Device for relaying Classic Bluetooth input as BLE
BLEServer* pServer;        // BLE Server to manage BLE connections

#define MAX_TRIED_DEVICES 50
String triedDevices[MAX_TRIED_DEVICES];  // Store tried device addresses
int triedDeviceCount = 0;

uint32_t lastConnectionAttempt = 0;
uint32_t heartbeat = 0;  // Declare heartbeat variable
#define HEARTBEAT_TIMEOUT_MS 5000  // Timeout constant for heartbeat

// Helper function to convert a MAC address to a string
String addressToString(esp_bd_addr_t addr) {
  char str[18];  // MAC address as string "XX:XX:XX:XX:XX:XX"
  snprintf(str, sizeof(str), "%02X:%02X:%02X:%02X:%02X:%02X", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
  return String(str);
}

// Check if the device is already in the tried list
bool isDeviceInTriedList(String addr) {
  for (int i = 0; i < triedDeviceCount; i++) {
    if (triedDevices[i] == addr) {
      return true;  // Device already tried
    }
  }
  return false;
}

// Add a device to the tried list
void addDeviceToTriedList(String addr) {
  if (triedDeviceCount < MAX_TRIED_DEVICES) {
    triedDevices[triedDeviceCount] = addr;  // Add the address as a string
    triedDeviceCount++;
  }
}

// Function to extract device name from EIR data
String getDeviceNameFromEIR(uint8_t *eir, uint8_t eir_len) {
  uint8_t length = 0;
  uint8_t* name_data = esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_CMPL_LOCAL_NAME, &length);

  if (name_data && length > 0) {
    char deviceName[248];  // Ensure the buffer is large enough
    strncpy(deviceName, (char*)name_data, length);  // Copy the data into the deviceName buffer
    deviceName[length] = '\0';  // Null-terminate the string
    return String(deviceName);
  }

  return String("(unknown)");
}

// Display function to update the screen, showing name first, then properties
void displayInfo(const String& deviceName, const String& deviceAddr, const String& rawCod, const String& deviceClass, const char* incomingChar = nullptr) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x8_tr);  // Choose a font

  // Ensure we fit the content within 25-character width and 5-line height
  if (!deviceName.isEmpty() && !deviceAddr.isEmpty()) {
    u8g2.setCursor(0, 10);
    u8g2.print("Name: ");
    if (deviceName.length() > 25) {
      u8g2.print(deviceName.substring(0, 25));  // Truncate name if it's too long
    } else {
      u8g2.print(deviceName);
    }

    u8g2.setCursor(0, 20);
    u8g2.print("Addr: ");
    u8g2.print(deviceAddr);  // Address fits in one line (max 17 characters)

    u8g2.setCursor(0, 30);
    u8g2.print("Raw CoD: ");
    u8g2.print(rawCod);  // Show the raw class of device (hex)

    u8g2.setCursor(0, 40);
    u8g2.print("Class: ");
    if (deviceClass.length() > 25) {
      u8g2.print(deviceClass.substring(0, 25));  // Truncate class if too long
    } else {
      u8g2.print(deviceClass);
    }
  } else if (incomingChar) {
    u8g2.setCursor(0, 10);
    u8g2.print("Received Key: ");
    u8g2.print(incomingChar);
  } else {
    u8g2.setCursor(0, 10);
    u8g2.print("Scanning...");
  }

  u8g2.sendBuffer();  // Transfer internal memory to the display
}

// Helper function to print detailed info including raw class of device
void printFullDeviceInfo(const String& deviceName, const String& deviceAddr, const String& rawCod, const String& deviceClass, int8_t rssi) {
  Serial.printf("Device Name: %s\n", deviceName.c_str());
  Serial.printf("Device Address: %s\n", deviceAddr.c_str());
  Serial.printf("Class of Device (Raw): %s\n", rawCod.c_str());
  Serial.printf("Class of Device: %s\n", deviceClass.c_str());
  Serial.printf("RSSI: %d dBm\n", rssi);
}

// Helper function to decode and print Class of Device (CoD) information
String getClassOfDeviceDescription(uint32_t cod) {
  String deviceClassStr;
  
  // Extract Major Service Class (bits 13 to 23)
  uint8_t majorServiceClass = (cod >> 13) & 0x7F;
  
  // Extract Major Device Class (bits 8 to 12)
  uint8_t majorDeviceClass = (cod >> 8) & 0x1F;
  
  // Extract Minor Device Class (bits 2 to 7)
  uint8_t minorDeviceClass = (cod >> 2) & 0x3F;

  // Append major service class description
  if (majorServiceClass & 0x20) deviceClassStr += "Audio, ";
  if (majorServiceClass & 0x40) deviceClassStr += "Telephony, ";
  if (majorServiceClass & 0x10) deviceClassStr += "Networking, ";
  if (majorServiceClass & 0x08) deviceClassStr += "Object Transfer, ";
  if (majorServiceClass & 0x04) deviceClassStr += "Capturing, ";
  if (majorServiceClass & 0x02) deviceClassStr += "Positioning, ";
  if (majorServiceClass & 0x01) deviceClassStr += "Limited Discovery, ";

  // Append major device class description
  deviceClassStr += "Major: ";
  switch (majorDeviceClass) {
    case 0x00: deviceClassStr += "Miscellaneous"; break;
    case 0x01: deviceClassStr += "Computer"; break;
    case 0x02: deviceClassStr += "Phone"; break;
    case 0x03: deviceClassStr += "LAN/Network"; break;
    case 0x04: deviceClassStr += "Audio/Video"; break;
    case 0x05: {  // Peripheral device, check for keyboard or mouse or combo
      deviceClassStr += "Peripheral (";
      switch (minorDeviceClass) {
        case 0x00: deviceClassStr += "Uncategorized"; break;
        case 0x01: deviceClassStr += "Keyboard"; break;
        case 0x02: deviceClassStr += "Mouse"; break;
        case 0x03: deviceClassStr += "Keyboard/Mouse Combo"; break;
        case 0x04: deviceClassStr += "Joystick"; break;
        case 0x05: deviceClassStr += "Gamepad"; break;
        case 0x06: deviceClassStr += "Remote Control"; break;
        case 0x07: deviceClassStr += "Sensing Device"; break;
        case 0x08: deviceClassStr += "Digitizer Tablet"; break;
        case 0x09: deviceClassStr += "Card Reader"; break;
        case 0x0A: deviceClassStr += "Digital Pen"; break;
        case 0x0B: deviceClassStr += "Handheld Scanner"; break;
        case 0x0C: deviceClassStr += "Handheld Gestural Input Device"; break;
        case 0x10: deviceClassStr += "Custom Keyboard/Mouse Combo"; break;  // Custom mapping for your device
        default: deviceClassStr += "Other Peripheral"; break;
      }
      deviceClassStr += ")";
      break;
    }
    case 0x06: deviceClassStr += "Imaging"; break;
    case 0x07: deviceClassStr += "Wearable"; break;
    case 0x08: deviceClassStr += "Toy"; break;
    case 0x09: deviceClassStr += "Health"; break;
    default: deviceClassStr += "Unknown"; break;
  }

  return deviceClassStr;
}



// Function to scan for Classic Bluetooth devices
void scanClassicBT() {
  if (!SerialBT.hasClient()) {
    Serial.println("Scanning for Classic Bluetooth devices...");
    displayInfo("", "", "", "");  // Show scanning message
    esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 10, 0);  // Start scanning
  }
}

// Callback when a device is found during scanning
void classicBTDeviceFound(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param) {
  if (event == ESP_BT_GAP_DISC_RES_EVT) {
    // Get device address directly from the 'bda' field
    String btDeviceAddr = addressToString(param->disc_res.bda);  // Get Bluetooth device address as a string

    // Check if the device is already in the tried list
    if (isDeviceInTriedList(btDeviceAddr)) {
      return;
    }

    // Add device to tried list
    addDeviceToTriedList(btDeviceAddr);

    // Extract the device name from EIR data
    String btDeviceName = "(unknown)";
    String deviceClassStr = "(unknown)";
    String rawCodStr = "(unknown)";
    int8_t rssi = -128;  // Default RSSI value if not provided
    uint32_t cod = 0;

    for (int i = 0; i < param->disc_res.num_prop; i++) {
      esp_bt_gap_dev_prop_t *p = &param->disc_res.prop[i];

      // Check the property type and print relevant information
      switch (p->type) {
        case ESP_BT_GAP_DEV_PROP_EIR: {
          uint8_t *eir = static_cast<uint8_t*>(p->val);
          uint8_t eir_len = p->len;
          btDeviceName = getDeviceNameFromEIR(eir, eir_len);
          break;
        }
        case ESP_BT_GAP_DEV_PROP_COD: {
          cod = *(uint32_t *)(p->val);  // Store Class of Device
          rawCodStr = String(cod, HEX);  // Store raw class of device in hex format
          deviceClassStr = getClassOfDeviceDescription(cod);  // Get human-readable class of device
          break;
        }
        case ESP_BT_GAP_DEV_PROP_RSSI: {
          rssi = *(int8_t *)(p->val);  // Store RSSI value
          break;
        }
        default:
          Serial.println("Unknown property type.");
          break;
      }
    }

    // Print the device information
    printFullDeviceInfo(btDeviceName, btDeviceAddr, rawCodStr, deviceClassStr, rssi);
    displayInfo(btDeviceName, btDeviceAddr, rawCodStr, deviceClassStr);  // Display detected device info on the screen

    // Check if the Major Device Class is Peripheral (0x05) and if the Minor Device Class is a keyboard or combo
    uint8_t majorDeviceClass = (cod >> 8) & 0x1F;  // Extract Major Device Class
    uint8_t minorDeviceClass = (cod >> 2) & 0x3F;  // Extract Minor Device Class

    if (majorDeviceClass == 0x05 && (minorDeviceClass == 0x01 || minorDeviceClass == 0x03 || minorDeviceClass == 0x10)) {
      Serial.printf("Connecting to Bluetooth Keyboard: %s\n", btDeviceName.c_str());

      // Attempt to connect to the device
      if (!SerialBT.connected()) {
        SerialBT.connect(btDeviceAddr.c_str());
        delay(5000);  // Give time for connection attempt
        if (SerialBT.connected()) {
          Serial.println("Connected to the Bluetooth keyboard!");
        } else {
          Serial.println("Failed to connect.");
        }
      }
    }
  }
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

  // Initialize GAP callback for Classic Bluetooth device discovery
  esp_bt_gap_register_callback(classicBTDeviceFound);

  Serial.println("Classic Bluetooth started. Scanning for keyboards...");
  displayInfo("", "", "", "");  // Initial display message

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

  // Start scanning for Classic Bluetooth devices
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
      displayInfo("", "", "", "", &incomingChar);

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
