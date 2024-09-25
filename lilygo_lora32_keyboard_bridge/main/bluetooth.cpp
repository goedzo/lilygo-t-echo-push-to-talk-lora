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
    checkAndConnectKeyboard(btDeviceAddr, cod, btDeviceName);
  }
}

void checkAndConnectKeyboard(const String& btDeviceAddr, uint32_t cod, const String& btDeviceName) {
  uint8_t majorDeviceClass = (cod >> 8) & 0x1F;
  uint8_t minorDeviceClass = (cod >> 2) & 0x3F;

  if (majorDeviceClass == 0x05 && (minorDeviceClass == 0x01 || minorDeviceClass == 0x03 || minorDeviceClass == 0x10)) {
    Serial.printf("Connecting to Bluetooth Keyboard: %s\n", btDeviceName.c_str());
    if (!SerialBT.connected()) {
      SerialBT.connect(btDeviceAddr.c_str());
      delay(5000);
      SerialBT.connected() ? Serial.println("Connected!") : Serial.println("Failed to connect.");
    }
  }
}
