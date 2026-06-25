#include "ble.h"
#include <bluefruit.h>
#include "display.h"
#include "app_modes.h"

// Create BLE service and characteristic
BLEService bleService("1235");
BLECharacteristic bleCharacteristic("ABCE");

void onCharacteristicWritten(uint16_t conn_handle, BLECharacteristic* chr, uint8_t* data, uint16_t len);
void onConnect(uint16_t conn_handle);
void onDisconnect(uint16_t conn_handle, uint8_t reason);

// Renamed helper function to check if the data contains printable characters
bool isDataPrintable(const uint8_t* data, int length);

// Get short device ID for beacon (last 8 hex chars of MAC)
const char* bleGetDeviceIdShort();

// Notification queue — defer BLE notify calls to loop() to avoid hard faults
#define NOTIF_QUEUE_SIZE 8
static char notif_queue[NOTIF_QUEUE_SIZE][102];
static uint8_t notif_queue_len[NOTIF_QUEUE_SIZE];
static uint8_t notif_head = 0;
static uint8_t notif_tail = 0;
static volatile bool notif_queue_empty = true;

static bool queueFull() {
    uint8_t next = (notif_head + 1) % NOTIF_QUEUE_SIZE;
    return next == notif_tail;
}

static bool enqueue(const char* msg) {
    if (queueFull()) return false;
    size_t len = strlen(msg);
    if (len >= sizeof(notif_queue[0])) return false;
    memcpy(notif_queue[notif_head], msg, len + 1);
    notif_queue_len[notif_head] = (uint8_t)(len + 1);
    notif_head = (notif_head + 1) % NOTIF_QUEUE_SIZE;
    notif_queue_empty = false;
    return true;
}

static void drainQueue() {
    if (notif_queue_empty) return;
    
    while (!notif_queue_empty && notif_head != notif_tail) {
        bool drained = false;
        for (int i = 0; i < NOTIF_QUEUE_SIZE && !notif_queue_empty; i++) {
            if (i == notif_tail && !queueFull() || (!queueFull() && i == notif_tail)) {
                uint8_t msg_len = notif_queue_len[i];
                size_t total = msg_len + 2; // +2 for "~~"
                uint8_t buffer[total];
                memcpy(buffer, notif_queue[i], msg_len);
                buffer[msg_len] = '~';
                buffer[msg_len + 1] = '~';
                
                if (bleCharacteristic.notify(buffer, total) == ERROR_NONE) {
                    drained = true;
                    delay(50); // Small gap between notifies to avoid SD queue overflow
                    
                    notif_queue[notif_tail][0] = '\0';
                    notif_queue_len[notif_tail] = 0;
                    notif_tail = (notif_tail + 1) % NOTIF_QUEUE_SIZE;
                    
                    if (notif_head == notif_tail) {
                        notif_queue_empty = true;
                    }
                } else {
                    // BLE not ready yet, stop draining
                    break;
                }
            }
        }
        if (!drained) break;
    }
}

void setupBLE() {
    Bluefruit.begin();
    Bluefruit.setTxPower(4);  // Set the TX power to max (4dBm)

    char deviceName[25];
    uint64_t mac = NRF_FICR->DEVICEID[0];
    snprintf(deviceName, sizeof(deviceName), "LilygoT-Echo-%08X", (unsigned long)mac);
    Bluefruit.setName(deviceName);

    // Drain USB before heavy BLE init to prevent blocking
    while (Serial.available()) Serial.read();
    delay(10);

    bleService.begin();

    bleCharacteristic.setProperties(CHR_PROPS_READ | CHR_PROPS_WRITE | CHR_PROPS_NOTIFY);
    bleCharacteristic.setPermission(SECMODE_OPEN, SECMODE_OPEN);
    bleCharacteristic.setMaxLen(100);
    bleCharacteristic.setFixedLen(100);
    bleCharacteristic.setWriteCallback(onCharacteristicWritten);
    
    while (Serial.available()) Serial.read();
    delay(10);

    bleCharacteristic.begin();

    Bluefruit.Periph.setConnectCallback(onConnect);
    Bluefruit.Periph.setDisconnectCallback(onDisconnect);

    Bluefruit.Advertising.addService(bleService);
    Bluefruit.Advertising.addName();

    while (Serial.available()) Serial.read();
    delay(10);

    Bluefruit.Advertising.start();

    // Drain USB after advertising starts
    while (Serial.available()) Serial.read();

    Serial.print("BLE Initialized with device name: ");
    Serial.println(deviceName);
}

void handleBLE() {
    drainQueue();
}

void onConnect(uint16_t conn_handle) {
    Serial.println("Phone connected!");
    //We need to wait until it is initialized before we send the screen contents.
    delay(1000);
    //Refresh screen to show it in app
    updModeAndChannelDisplay();


}

void onDisconnect(uint16_t conn_handle, uint8_t reason) {
    Serial.println("Phone disconnected!");
}

void onCharacteristicWritten(uint16_t conn_handle, BLECharacteristic* chr, uint8_t* data, uint16_t len) {
    // This callback is invoked when the characteristic is written to
    Serial.print("Characteristic written, length: ");
    Serial.println(len);

    // Check if the data is fully printable
    if (isDataPrintable(data, len)) {
        // Convert byte array to String assuming it is printable
        String receivedValue;
        for (int i = 0; i < len; i++) {
            receivedValue += (char)data[i];
        }
        Serial.print("Received string: ");
        Serial.println(receivedValue);

        // Process the received message if it contains a ':'
        int delimiterIndex = receivedValue.indexOf(':');
        if (delimiterIndex != -1) {
            String action = receivedValue.substring(0, delimiterIndex);
            String value = receivedValue.substring(delimiterIndex + 1);

            Serial.print("Action: ");
            Serial.println(action);
            Serial.print("Value: ");
            Serial.println(value);

            // Handle the action and value accordingly
            if (action == "SETMODE") {
                switchMode(value);
            } 
            else if (action == "SENDTXT") {
                sendTxtMessage(value.c_str());
            } 
            else if (action == "GETSTATUS") {
                // Return connection status: BLE link + LoRa peer liveness
                // If we're in the write callback, BLE is connected by definition.
                extern bool isPeerAlive();
                bool loraAlive = isPeerAlive();
                
                char resp[32];
                snprintf(resp, sizeof(resp), "OK{BLE:1}{LORA:%d}", loraAlive ? 1 : 0);
                sendNotificationToApp(resp);
            } 
            else {
                Serial.println("Unknown action");
            }
        } else {
            // No delimiter — check for bare GETSTATUS
            if (receivedValue == "GETSTATUS") {
                extern bool isPeerAlive();
                bool loraAlive = isPeerAlive();
                
                char resp[32];
                snprintf(resp, sizeof(resp), "OK{BLE:1}{LORA:%d}", loraAlive ? 1 : 0);
                sendNotificationToApp(resp);
            } else {
                Serial.println("Invalid format. Missing ':' delimiter.");
            }
        }

    } else if (len >= 4 && data[0] == 0xFE && data[1] == 0x01) {
        // Handle as binary Opus audio frame
        uint16_t opusLen = ((uint16_t)data[3] << 8) | data[2];
        if (opusLen > 0 && 4 + opusLen <= len) {
            extern void sendPacket(uint8_t* pkt_buf, uint16_t len, unsigned int messageCounterOverride);
            
            // Build LoRa packet: PT{channel}{O}{opus_bytes}
            static char opusPktBuf[MAX_PKT];
            snprintf(opusPktBuf, sizeof(opusPktBuf), "PT%cO", 
                     channels[deviceSettings.channel_idx]);
            
            int pktLen = strlen(opusPktBuf);
            memcpy(opusPktBuf + pktLen, (char*)(data + 4), opusLen);
            pktLen += opusLen;
            
            sendPacket((uint8_t*)opusPktBuf, (uint16_t)pktLen, 0);
        }

    } else {
        // Handle as binary data (unknown type)
        Serial.print("Received binary data: ");
        for (int i = 0; i < len; i++) {
            Serial.print("0x");
            if (data[i] < 16) Serial.print("0");
            Serial.print(data[i], HEX);
            Serial.print(" ");
        }
        Serial.println();
    }
}

// Function to send a notification to the app — queues for deferred delivery
void sendNotificationToApp(const char* message) {
    if (!message || !message[0]) return;
    
    size_t msgLen = strlen(message);
    
    // Create buffer with "~~" terminator
    uint8_t buffer[msgLen + 3];
    memcpy(buffer, message, msgLen);
    buffer[msgLen] = '~';
    buffer[msgLen + 1] = '~';
    buffer[msgLen + 2] = '\0';

    // Build notification string for queue: "LINE:XX|TEXT:buf"
    char notifStr[130];
    snprintf(notifStr, sizeof(notifStr), "LINE:NOTIF|DATA:%.*s", (int)(msgLen + 2), buffer);
    
    if (!enqueue(notifStr)) {
        // Queue full — try to drain first
        drainQueue();
        enqueue(notifStr);
    }
}

// Helper function to check if the data contains printable characters
bool isDataPrintable(const uint8_t* data, int length) {
    for (int i = 0; i < length; i++) {
        if (!isPrintable(data[i])) {  // Use built-in Arduino isPrintable for individual bytes
            return false;  // Non-printable character found
        }
    }
    return true;
}

// Global to store the short device ID derived from MAC
static char deviceIdShort[9];

const char* bleGetDeviceIdShort() {
    if (deviceIdShort[0] == '\0') {
        uint64_t mac = NRF_FICR->DEVICEID[0];
        snprintf(deviceIdShort, sizeof(deviceIdShort), "%08X", (unsigned long)mac);
    }
    return deviceIdShort;
}
