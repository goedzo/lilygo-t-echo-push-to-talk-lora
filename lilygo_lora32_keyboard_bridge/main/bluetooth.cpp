#include "bluetooth.h"
#include "utility.h"
#include "display.h"
#include "BluetoothSerial.h"
#include "esp_bt.h"
#include "esp_bt_device.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_hidh.h"  // HID Host API
#include "esp_hidh_api.h"  // Include HID API
#include "esp_log.h"

#define TAG "HID_HOST"
#define TAG "SPP_HOST"


BluetoothSerial SerialBT;  // Classic Bluetooth object for scanning and connections

// HID Host Callback to handle HID device events
void hid_event_handler(esp_hidh_cb_event_t event, esp_hidh_cb_param_t *param);
void spp_event_handler(esp_spp_cb_event_t event, esp_spp_cb_param_t *param);

void initBluetooth() {
    esp_log_level_set("BT", ESP_LOG_DEBUG);

    // Initialize Classic Bluetooth
    if (!SerialBT.begin("ESP32_BT_Classic")) {
        Serial.println("An error occurred initializing Classic Bluetooth");
        return;
    }

    // Register gap callback for Classic Bluetooth
    esp_bt_gap_register_callback(classicBTDeviceFound);

    // Initialize Bluedroid stack
    esp_err_t ret = esp_bluedroid_init();
    if (ret != ESP_OK) {
        Serial.printf("Bluedroid initialize failed: %s\n", esp_err_to_name(ret));
        return;
    }

    ret = esp_bluedroid_enable();
    if (ret != ESP_OK) {
        Serial.printf("Bluedroid enable failed: %s\n", esp_err_to_name(ret));
        return;
    }


    // Initialize SPP (Serial Port Profile)
    ret = esp_spp_register_callback(spp_event_handler);
    if (ret != ESP_OK) {
        Serial.printf("SPP callback registration failed: %s\n", esp_err_to_name(ret));
        return;
    }

    ret = esp_spp_init(ESP_SPP_MODE_CB);
    if (ret != ESP_OK) {
        Serial.printf("SPP init failed: %s\n", esp_err_to_name(ret));
        return;
    }

    Serial.println("Classic Bluetooth started. Scanning for devices...");
    SerialBT.enableSSP();  // Enable Secure Simple Pairing
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

        if (btDeviceName == "(unknown)") {
            btDeviceName = getDeviceNameFromEIR(static_cast<uint8_t*>(param->disc_res.prop[0].val), param->disc_res.prop[0].len);
        }

        printFullDeviceInfo(btDeviceName, btDeviceAddr, rawCodStr, deviceClassStr, rssi);
        displayInfo(btDeviceName, btDeviceAddr, rawCodStr, deviceClassStr);

        checkAndConnectKeyboard(btDeviceAddr, cod, btDeviceName, rssi);
    }
}


// Callback to handle HID events such as connection, disconnection, and data
void hid_event_handler(esp_hidh_cb_event_t event, esp_hidh_cb_param_t *param) {
    switch (event) {
        case ESP_HIDH_OPEN_EVT:
            if (param->open.status == ESP_OK) {
                Serial.println("HID device connected.");
            } else {
                Serial.println("Failed to connect to HID device.");
            }
            break;
        case ESP_HIDH_CLOSE_EVT:
            Serial.println("HID device disconnected.");
            break;
        default:
            break;
    }
}


// SPP event handler
void spp_event_handler(esp_spp_cb_event_t event, esp_spp_cb_param_t *param) {
    switch (event) {
        case ESP_SPP_INIT_EVT:
            Serial.println("SPP initialized successfully.");
            break;
        case ESP_SPP_DISCOVERY_COMP_EVT:
            Serial.println("SPP device discovery completed.");
            break;
        case ESP_SPP_OPEN_EVT:
            Serial.println("SPP connection opened.");
            break;
        case ESP_SPP_CLOSE_EVT:
            Serial.println("SPP connection closed.");
            break;
        case ESP_SPP_DATA_IND_EVT:
            Serial.printf("SPP data received: %.*s\n", param->data_ind.len, (char *)param->data_ind.data);
            break;
        default:
            Serial.println("Unhandled SPP event");
            break;
    }
}


// Function to handle Bluetooth PIN code requests (optional for keyboards that require a PIN)
void btPinCodeRequestCallback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param) {
    if (event == ESP_BT_GAP_PIN_REQ_EVT) {
        Serial.println("Bluetooth PIN Code Request received.");

        // Set a default PIN code (if necessary, you can also ask the user for input)
        uint8_t pinCode[4] = { '1', '2', '3', '4' };  // Example PIN: 1234
        esp_bt_gap_pin_reply(param->pin_req.bda, true, 4, pinCode);
        Serial.println("PIN code '1234' sent for pairing.");
    }
}

// Function to initiate pairing with the device
void initBluetoothPairing() {
    uint8_t ioCapability = ESP_BT_IO_CAP_NONE;  // No input/output capabilities
    esp_bt_gap_set_security_param(ESP_BT_SP_IOCAP_MODE, &ioCapability, sizeof(uint8_t));

    esp_bt_dev_set_device_name("ESP32_Keyboard");
    esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
    esp_bt_gap_register_callback(btPinCodeRequestCallback);
    esp_bt_gap_set_pin(ESP_BT_PIN_TYPE_VARIABLE, 0, nullptr);
}


// Function to remove a bonded device
void removeBondedDevice(const String& btDeviceAddrStr) {
    esp_bd_addr_t btDeviceAddr;
    // Convert the string address "XX:XX:XX:XX:XX:XX" to byte array
    sscanf(btDeviceAddrStr.c_str(), "%02X:%02X:%02X:%02X:%02X:%02X",
           (unsigned int*)&btDeviceAddr[0],
           (unsigned int*)&btDeviceAddr[1],
           (unsigned int*)&btDeviceAddr[2],
           (unsigned int*)&btDeviceAddr[3],
           (unsigned int*)&btDeviceAddr[4],
           (unsigned int*)&btDeviceAddr[5]);

    // Remove the bonded device
    esp_bt_gap_remove_bond_device(btDeviceAddr);
}

void initiatePairing(const String& btDeviceAddr) {


    esp_bd_addr_t addr;
    sscanf(btDeviceAddr.c_str(), "%02X:%02X:%02X:%02X:%02X:%02X",
           (unsigned int*)&addr[0],
           (unsigned int*)&addr[1],
           (unsigned int*)&addr[2],
           (unsigned int*)&addr[3],
           (unsigned int*)&addr[4],
           (unsigned int*)&addr[5]);

    esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);

    // Reply with a default PIN if necessary (this part can be skipped for most devices)
    esp_bt_gap_pin_reply(addr, true, 0, nullptr);  // Attempt pairing without PIN
}


// Updated function to check and connect to the keyboard with pairing support
// Check if the device is a keyboard and initiate connection
void checkAndConnectKeyboard(const String& btDeviceAddr, uint32_t cod, const String& btDeviceName, int8_t rssi) {
    uint8_t majorDeviceClass = (cod >> 8) & 0x1F;
    uint8_t minorDeviceClass = (cod >> 2) & 0x3F;

    if (majorDeviceClass == 0x05 && (minorDeviceClass == 0x01 || minorDeviceClass == 0x03 || minorDeviceClass == 0x10)) {
        Serial.printf("Attempting to pair and connect to Bluetooth Keyboard: %s\n", btDeviceName.c_str());

        if (!SerialBT.connected()) {
            Serial.printf("Device Address: %s, Class of Device: 0x%X, RSSI: %d dBm\n", btDeviceAddr.c_str(), cod, rssi);

            initBluetoothPairing();
            removeBondedDevice(btDeviceAddr);
            initiatePairing(btDeviceAddr);

            uint8_t addr[6];
            sscanf(btDeviceAddr.c_str(), "%02X:%02X:%02X:%02X:%02X:%02X",
                   (unsigned int*)&addr[0],
                   (unsigned int*)&addr[1],
                   (unsigned int*)&addr[2],
                   (unsigned int*)&addr[3],
                   (unsigned int*)&addr[4],
                   (unsigned int*)&addr[5]);

            int retries = 3;
            bool connected = false;
            while (retries-- > 0 && !connected) {
                Serial.printf("Initiating connection to %s (%d retries left)...\n", btDeviceAddr.c_str(), retries);

                if (SerialBT.connect(addr, 1, ESP_SPP_SEC_ENCRYPT | ESP_SPP_SEC_AUTHENTICATE, ESP_SPP_ROLE_MASTER)) {
                    Serial.println("Connected successfully!");
                    connected = true;
                } else {
                    Serial.println("Failed to connect.");
                    delay(10000);
                }
            }

            if (!connected) {
                Serial.println("Failed to connect after multiple attempts.");
            }
        } else {
            Serial.println("Already connected to a device.");
        }
    } else {
        Serial.println("Device is not a Bluetooth keyboard.");
    }
}
