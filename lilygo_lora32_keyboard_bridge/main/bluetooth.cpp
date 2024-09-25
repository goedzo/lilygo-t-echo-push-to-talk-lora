#include "bluetooth.h"
#include "utility.h"

BluetoothSerial SerialBT;  // Classic Bluetooth object for scanning and connections

void initBluetooth() {
  if (!SerialBT.begin("ESP32_BT_Classic")) {
    Serial.println("An error occurred initializing Classic Bluetooth");
    return;
  }
  esp_bt_gap_register_callback(classicBTDeviceFound);
  Serial.println("Classic Bluetooth started. Scanning for keyboards...");
  displayInfo("", "", "", "");  // Initial display message
}

void scanClassicBT() {
  if (!SerialBT.hasClient()) {
    Serial.println("Scanning for Classic Bluetooth devices...");
    displayInfo("", "", "", "");  // Show scanning message
    esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 10, 0);  // Start scanning
  }
}

// Callback when a Classic Bluetooth device is found
void classicBTDeviceFound(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param) {
  if (event == ESP_BT_GAP_DISC_RES_EVT) {
    String btDeviceAddr = addressToString(param->disc_res.bda);
    if (isDeviceInTriedList(btDeviceAddr)) return;
    addDeviceToTriedList(btDeviceAddr);

    String btDeviceName = "(unknown)";
    String deviceClassStr = "(unknown)";
    String rawCodStr = "(unknown)";
    int8_t rssi = -128;
    uint32_t cod = 0;

    for (int i = 0; i < param->disc_res.num_prop; i++) {
      esp_bt_gap_dev_prop_t *p = &param->disc_res.prop[i];
      switch (p->type) {
        case ESP_BT_GAP_DEV_PROP_EIR:
          btDeviceName = getDeviceNameFromEIR(static_cast<uint8_t*>(p->val), p->len);
          break;
        case ESP_BT_GAP_DEV_PROP_COD:
          cod = *(uint32_t *)(p->val);
          rawCodStr = String(cod, HEX);
          deviceClassStr = getClassOfDeviceDescription(cod);
          break;
        case ESP_BT_GAP_DEV_PROP_RSSI:
          rssi = *(int8_t *)(p->val);
          break;
      }
    }
    printFullDeviceInfo(btDeviceName, btDeviceAddr, rawCodStr, deviceClassStr, rssi);
    displayInfo(btDeviceName, btDeviceAddr, rawCodStr, deviceClassStr);
    checkAndConnectKeyboard(btDeviceAddr, cod, btDeviceName,rssi);
  }
}

void checkAndConnectKeyboard(const String& btDeviceAddr, uint32_t cod, const String& btDeviceName, int8_t rssi) {
  uint8_t majorDeviceClass = (cod >> 8) & 0x1F;
  uint8_t minorDeviceClass = (cod >> 2) & 0x3F;

  if (majorDeviceClass == 0x05 && (minorDeviceClass == 0x01 || minorDeviceClass == 0x03 || minorDeviceClass == 0x10)) {
    Serial.printf("Attempting to connect to Bluetooth Keyboard: %s\n", btDeviceName.c_str());

    if (!SerialBT.connected()) {
      // Log the Bluetooth address and class of device
      Serial.printf("Device Address: %s, Class of Device: 0x%X, RSSI: %d dBm\n", btDeviceAddr.c_str(), cod, rssi);
      
      // Initiate connection
      Serial.printf("Initiating connection to %s...\n", btDeviceAddr.c_str());
      SerialBT.connect(btDeviceAddr.c_str());
      
      // Wait for 5 seconds to check if connection is successful
      delay(5000);
      
      if (SerialBT.connected()) {
        Serial.println("Connected successfully!");
      } else {
        // Log more details on failure
        Serial.println("Failed to connect.");
        
        // Check for possible reasons for failure
        if (rssi < -75) {
          Serial.println("Potential reason: Signal strength too weak (RSSI < -75 dBm).");
        } else {
          Serial.println("Potential reason: Device may not be accepting connections or busy.");
        }

        // Additional diagnostics (if possible)
        // Check if the device is still visible or if the device has disappeared
        if (!SerialBT.hasClient()) {
          Serial.println("Potential reason: Device no longer visible or out of range.");
        } else {
          Serial.println("Potential reason: Pairing issue or unsupported Bluetooth protocol.");
        }
      }
    } else {
      Serial.println("Already connected to a device.");
    }
  } else {
    Serial.println("Device is not a Bluetooth keyboard.");
  }
}

