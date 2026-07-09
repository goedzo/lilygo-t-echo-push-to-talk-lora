#include "ble.h"
#include <bluefruit.h>
#include "utilities.h"
#include "display.h"
#include "app_modes.h"
#include "buddy_list.h"
#include "display_layout.h"

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

// BLE connection state — set by onConnect/onDisconnect
static bool ble_connected = false;

// Deferred display update flag — avoids calling display functions inside SoftDevice callback context
static volatile bool pending_display_update = false;

// Notification queue — defer BLE notify calls to loop() to avoid hard faults
// Increased from 8→16 and 256→320 bytes per slot to handle high-frequency SCAN data
#define NOTIF_QUEUE_SIZE 16
#define NOTIF_SLOT_SIZE  320
#define BIN_NOTIF_QUEUE_SIZE 4
static char notif_queue[NOTIF_QUEUE_SIZE][NOTIF_SLOT_SIZE];
static uint8_t notif_queue_len[NOTIF_QUEUE_SIZE];
static uint8_t notif_head = 0;
static uint8_t notif_tail = 0;
static volatile bool notif_queue_empty = true;

// Track if we're inside the write callback — prevents SoftDevice deadlock
static volatile bool in_write_callback = false;

// Fragmented notification queue — holds split payloads for multi-MTU messages
// Each item: "LINE:BLOB|S{seq}|T{total}|<data>~~" (header) or "LINE:BLOB|S{seq}|<data>~~" (continuation)
#define BLOB_NOTIF_QUEUE_SIZE 32
static char blob_notif_queue[BLOB_NOTIF_QUEUE_SIZE][NOTIF_SLOT_SIZE];
static uint8_t blob_notif_len[BLOB_NOTIF_QUEUE_SIZE];
static uint8_t blob_notif_head = 0;
static uint8_t blob_notif_tail = 0;
static volatile bool blob_notif_empty = true;

// Stall queue/drain processing during connection handshake — SoftDevice needs time to stabilize
uint32_t ble_connect_stall_until = 0;
#define BLE_CONNECT_STALL_MS 1500

// Track stale drain failures — if notify() keeps failing, clear the stale item after timeout
static volatile bool drain_failed = false;
static uint32_t drain_fail_start = 0;
static uint32_t drain_stall_until = 0;
#define DRAIN_FAIL_TIMEOUT_MS   1500    // Clear stale items after 1.5s of persistent failures
#define DRAIN_STALL_MS          3000    // Silent stall for 3s after first failure

// Maximum BLE ATT MTU we should send per notification (negotiated with central)
#define MAX_BLE_MTU 247

// Notification string buffer size: PREFIX(16) + payload(230) + ~~(2) + margin = 252
// Used by both sendSerialToApp() and sendNotificationToApp()
#define NOTIF_STR_MAX_LEN (16 + 230 + 2 + 4)

// Map Nordic softdevice error codes to human-readable names
static const char* bleErrorName(int32_t err) {
    switch ((uint32_t)err) {
        case 0:        return "ERROR_NONE";
        case 0x10001:  return "NRF_ERROR_SVC_HANDLER_ALLOC_FAILED";
        case 0x10002:  return "NRF_ERROR_SOFTDEVICE_NOT_PRESENT";
        case 0x10003:  return "NRF_ERROR_STACK_SIZE";
        case 0x2001:   return "NRF_ERROR_INVALID_ADDR";
        case 0x2002:   return "NRF_ERROR_NULL";
        case 0x2003:   return "NRF_ERROR_INVALID_PARAM";
        case 0x2004:   return "NRF_ERROR_INVALID_STATE";
        case 0x2005:   return "NRF_ERROR_INVALID_LENGTH";
        case 0x2006:   return "NRF_ERROR_INVALID_FLAGS";
        case 0x2007:   return "NRF_ERROR_INVALID_DATA";
        case 0x200B:   return "NRF_ERROR_NO_MEM";      // No memory available
        case 0x3001:   return "NRF_ERROR_BUSY";
        case 0x3008:   return "BLE_ERROR_INVALID_DB_IDX";
        case 0x3009:   return "BLE_ERROR_INVALID_CONN_HANDLE";
        case 0x300A:   return "BLE_ERROR_ATTR_DATA_SIZE"; // Attribute data too small
        case 0x300B:   return "BLE_ERROR_UNSUPPORTED_PRF";
        case 0x300C:   return "BLE_ERROR_INVALID_ADDR";
        case 0x4001:   return "NRF_ERROR_COMMUNICATION";
        default:       return "UNKNOWN_ERR";
    }
}

// Fragmented notification — each fragment carries exactly 64 bytes of raw message data.
// Fragment format: "LINE:BLOB|S{seq}|<raw_data>~~" (~79 bytes total per fragment)
#define BLOB_NOTIF_QUEUE_SIZE 32 // slots for fragmented items

// Deferred screen sync — avoid dropping in write callback (in_write_callback causes sendNotificationToApp to return immediately)
static volatile bool pending_screen_sync = false;

// Extern for sendSerialToAppLn in screen_sync.cpp — used by drain failure logging
extern void sendSerialToAppLn(const String& msg);

// Binary notification queue for Opus frames (raw bytes, no text wrapping)
static uint8_t bin_notif_queue[BIN_NOTIF_QUEUE_SIZE][128];
static uint8_t bin_notif_len[BIN_NOTIF_QUEUE_SIZE];
static uint8_t bin_notif_head = 0;
static uint8_t bin_notif_tail = 0;

// Return count of pending notifications in queue
int getPendingNotificationCount() {
    if (notif_queue_empty) return 0;
    int count = notif_head >= notif_tail ? (notif_head - notif_tail) : (NOTIF_QUEUE_SIZE - notif_tail + notif_head);
    if (count == 0 && !notif_queue_empty) count = NOTIF_QUEUE_SIZE - 1;
    return count;
}

static bool queueFull() {
    uint8_t next = (notif_head + 1) % NOTIF_QUEUE_SIZE;
    return next == notif_tail;
}

static bool enqueue(const char* msg) {
    if (queueFull()) return false;
    size_t len = strlen(msg);
    if (len >= NOTIF_SLOT_SIZE) return false;
    memcpy(notif_queue[notif_head], msg, len + 1);
    notif_queue_len[notif_head] = (uint8_t)(len + 1);
    notif_head = (notif_head + 1) % NOTIF_QUEUE_SIZE;
    notif_queue_empty = false;
    
    // Debug: log enqueue for LINE:SERIAL messages
    if (strncmp(msg, "LINE:SERIAL", 11) == 0) {
        SerialMon.print(F("[BLE] enqueue head="));
        SerialMon.print(notif_head);
        SerialMon.print(" len=");
        SerialMon.print(len);
        SerialMon.print(" stall_until=");
        SerialMon.print(drain_stall_until ? drain_stall_until : 0);
        SerialMon.println();
    }
    return true;
}

// Drain all pending text notifications — handles both regular queue and blob fragments
static void drainQueue() {
    if (notif_queue_empty && blob_notif_empty) return;

    // Stall silently during connection handshake — SoftDevice needs time to stabilize
    if (millis() < ble_connect_stall_until) return;
    
    // Stall silently after drain failure — prevents hammering notify() when central can't keep up
    if (drain_failed && millis() < drain_stall_until) return;

    // Drain all pending items — don't limit to 1 per call.
    // Priority: drain blob fragments first, then regular notifications
    uint8_t iter = 0;
    
    while (!notif_queue_empty || !blob_notif_empty) {
        if (iter++ > 40) { SerialMon.println(F("[BLE] drainQueue: infinite loop detected, breaking")); break; }

        // Prefer blob fragments when available (they're time-sensitive screen updates)
        bool useBlob = !blob_notif_empty && notif_queue_empty;
        
        uint8_t buffer[MAX_BLE_MTU];
        uint8_t msg_len;
        size_t total;
        uint8_t idx;
        
        if (useBlob) {
            msg_len = blob_notif_len[blob_notif_tail];
            total = msg_len + 2; // +2 for "~~"
            
            // Clamp payload to max BLE MTU
            if (total > MAX_BLE_MTU) {
                msg_len = (uint8_t)(MAX_BLE_MTU - 2);
                total = msg_len + 2;
            }

            if (msg_len == 0) {
                blob_notif_empty = true;
                blob_notif_tail = (blob_notif_tail + 1) % BLOB_NOTIF_QUEUE_SIZE;
                continue;
            }

            memcpy(buffer, blob_notif_queue[blob_notif_tail], msg_len);
        } else {
            idx = notif_tail;
            msg_len = notif_queue_len[idx];
            total = msg_len + 2;
            
            // Clamp payload to max BLE MTU
            if (total > MAX_BLE_MTU) {
                msg_len = (uint8_t)(MAX_BLE_MTU - 2);
                total = msg_len + 2;
            }

            // Sanity: skip empty slots
            if (msg_len == 0) {
                notif_queue_empty = true;
                break;
            }

            memcpy(buffer, notif_queue[idx], msg_len);
            buffer[msg_len] = '~';
            buffer[msg_len + 1] = '~';
        }

        // After stall timeout expires: clear stale item unconditionally to unblock queue
        if (drain_failed && millis() >= drain_stall_until) {
            if (useBlob) {
                blob_notif_queue[blob_notif_tail][0] = '\0';
                blob_notif_len[blob_notif_tail] = 0;
                blob_notif_tail = (blob_notif_tail + 1) % BLOB_NOTIF_QUEUE_SIZE;
                if (blob_notif_head == blob_notif_tail) {
                    blob_notif_empty = true;
                }
            } else {
                notif_queue[idx][0] = '\0';
                notif_queue_len[idx] = 0;
                notif_tail = (notif_tail + 1) % NOTIF_QUEUE_SIZE;
                if (notif_head == notif_tail) {
                    notif_queue_empty = true;
                }
            }
            drain_failed = false;
            drain_stall_until = 0;
            continue; // Try draining next item
        }

        int32_t ret = bleCharacteristic.notify(buffer, total);

        // Pace notifications: 50ms gap between each to avoid overwhelming the BLE connection event loop
        if (ret == ERROR_NONE) {
            uint32_t next_send_at = millis() + 50;
            while (millis() < next_send_at) { /* wait */ }

            drain_failed = false;
            drain_stall_until = 0;
            if (useBlob) {
                blob_notif_queue[blob_notif_tail][0] = '\0';
                blob_notif_len[blob_notif_tail] = 0;
                blob_notif_tail = (blob_notif_tail + 1) % BLOB_NOTIF_QUEUE_SIZE;
                if (blob_notif_head == blob_notif_tail) {
                    blob_notif_empty = true;
                }
            } else {
                notif_queue[idx][0] = '\0';
                notif_queue_len[idx] = 0;
                notif_tail = (notif_tail + 1) % NOTIF_QUEUE_SIZE;
                if (notif_head == notif_tail) {
                    notif_queue_empty = true;
                }
            }
        } else {
            // Track drain failures — set extended stall window to give central time to recover
            if (!drain_failed) {
                // First failure: log the error name, return code, payload length, and first 4 bytes of payload
                String msg = F("[BLE] drain fail: ");
                msg += bleErrorName(ret);
                msg += " code=";
                msg += ret;
                msg += " len=";
                msg += total;
                if (total >= 4) {
                    msg += " data=";
                    msg += (char)buffer[0];
                    msg += (char)buffer[1];
                    msg += (char)buffer[2];
                    msg += (char)buffer[3];
                }
                sendSerialToAppLn(msg);
                drain_failed = true;
                drain_fail_start = millis();
                drain_stall_until = millis() + DRAIN_STALL_MS;
            } else if (millis() - drain_fail_start > DRAIN_FAIL_TIMEOUT_MS) {
                // Clear stale notification — nothing is consuming it
                if (useBlob) {
                    blob_notif_queue[blob_notif_tail][0] = '\0';
                    blob_notif_len[blob_notif_tail] = 0;
                    blob_notif_tail = (blob_notif_tail + 1) % BLOB_NOTIF_QUEUE_SIZE;
                    if (blob_notif_head == blob_notif_tail) {
                        blob_notif_empty = true;
                    }
                } else {
                    notif_queue[notif_tail][0] = '\0';
                    notif_queue_len[notif_tail] = 0;
                    notif_tail = (notif_tail + 1) % NOTIF_QUEUE_SIZE;
                    if (notif_head == notif_tail) {
                        notif_queue_empty = true;
                    }
                }
                drain_failed = false;
                drain_stall_until = 0;
            } else {
                // Within stall window — break without re-hammering notify()
            }
            break; // Failed to send — stop draining, retry next iteration
        }
    }
}

// Fragmented notification — splits payloads exceeding MTU across BLE notifications
// All fragments share a uniform format so the companion app doesn't need special parsing:
//   "LINE:BLOB|<raw_payload_data>~~"
// The app strips the LINE:BLOB prefix, concatenates raw data in order (S0=first, S1=next...),
// and reconstructs the original message before processing.
bool sendFragmentedNotification(const char* message) {
    if (!message || !message[0]) return false;

    size_t msgLen = strlen(message);

    // Single notification fits — use normal path
    if (msgLen + 2 <= MAX_BLE_MTU) {
        static char notifStr[NOTIF_STR_MAX_LEN];
        snprintf(notifStr, sizeof(notifStr), "LINE:NOTIF|DATA:%.*s", (int)(msgLen + 2), message);
        return enqueue(notifStr);
    }

    // Multi-fragment mode — each fragment carries exactly MAX_BLOB_DATA bytes of raw payload.
    const int MAX_BLOB_DATA = 64; // ~64 bytes of original message data per fragment
    
    int totalFragments = (msgLen + MAX_BLOB_DATA - 1) / MAX_BLOB_DATA;
    if (totalFragments > 255 || totalFragments < 2) return false;

    for (int frag = 0; frag < totalFragments; frag++) {
        int dataOff = frag * MAX_BLOB_DATA;
        size_t remaining = msgLen - dataOff;
        size_t dataSz = remaining > MAX_BLOB_DATA ? MAX_BLOB_DATA : remaining;

        // Build fragment: "LINE:BLOB|S{seq}|<raw_data>~~"
        char buf[NOTIF_SLOT_SIZE];
        int off = sprintf(buf, "LINE:BLOB|S%d|", frag);
        memcpy(buf + off, message + dataOff, dataSz);
        off += (int)dataSz;
        buf[off++] = '~';
        buf[off++] = '~';
        buf[off] = '\0';

        blob_notif_queue[blob_notif_head][0] = '\0';
        memcpy(blob_notif_queue[blob_notif_head], buf, (uint8_t)(off + 1));
        blob_notif_len[blob_notif_head] = (uint8_t)off;
        blob_notif_head = (blob_notif_head + 1) % BLOB_NOTIF_QUEUE_SIZE;
        blob_notif_empty = false;
    }

    return true;
}
static bool binQueueFull() {
    uint8_t next = (bin_notif_head + 1) % BIN_NOTIF_QUEUE_SIZE;
    return next == bin_notif_tail;
}

static bool enqueueBinary(const uint8_t* data, uint8_t len) {
    if (len > 127) return false;
    if (binQueueFull()) return false;
    memcpy(bin_notif_queue[bin_notif_head], data, len);
    bin_notif_len[bin_notif_head] = len;
    bin_notif_head = (bin_notif_head + 1) % BIN_NOTIF_QUEUE_SIZE;
    return true;
}

static void drainBinaryQueue() {
    if (bin_notif_head == bin_notif_tail) return;
    
    uint8_t idx = bin_notif_tail;
    uint8_t len = bin_notif_len[idx];
    
    SerialMon.print(F("[BLE] drainBinary: idx="));
    SerialMon.print(idx);
    SerialMon.print(" len=");
    SerialMon.print(len);
    SerialMon.print(" notify()...");
    
    int32_t ret = bleCharacteristic.notify(bin_notif_queue[idx], len);
    
    SerialMon.print(ret == ERROR_NONE ? "ERROR_NONE" : String("ERR(") + ret + ")");
    SerialMon.println();
    
    if (ret == ERROR_NONE) {
        bin_notif_len[idx] = 0;
        bin_notif_tail = (bin_notif_tail + 1) % BIN_NOTIF_QUEUE_SIZE;
    }
}

void sendBinaryNotification(const uint8_t* data, uint8_t len) {
    if (!data || len == 0) return;
    enqueueBinary(data, len);
}

// Increase SoftDevice ATT MTU to 247 before BLE init (default is 23)
// Uses Bluefruit.configPrphConn() — must be called before Bluefruit.begin()
// This reduces BLE fragmentation from ~8 packets per message down to 1-2

void setupBLE() {
    // Set maximum ATT MTU via Bluefruit API before Bluefruit.begin()
    // Request max ATT MTU (247 bytes) for larger single-packet payloads — reduces fragmentation
    // Use explicit connection parameters: min_event_length=3, hvn_tx_queue=6
    Bluefruit.configPrphConn(247, BLE_GAP_EVENT_LENGTH_DEFAULT, 6, BLE_GATTC_WRITE_CMD_TX_QUEUE_SIZE_DEFAULT);

    // Request MTU override via GATT before advertising — central will negotiate this value
    uint16_t mtu = 247;
    (void)mtu; // Value set via configPrphConn ATT_MTU param above

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
    bleCharacteristic.setMaxLen(253);
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

    SerialMon.println("BLE Initialized with device name: " + String(deviceName));
    sendSerialToAppLn("[BOOT] BLE init complete");
}

// Serial log relay — queues device output to companion app LINE:SERIAL
static volatile bool serial_pending = false;

bool isPhoneConnected() {
    static uint8_t call_cnt = 0;
    if (call_cnt++ % 100 == 0) {
        SerialMon.print(F("[BLE] isPhoneConnected(): notif_queue_empty="));
        SerialMon.print(notif_queue_empty ? 1 : 0);
        SerialMon.print(" head=");
        SerialMon.print(notif_head);
        SerialMon.print(" tail=");
        SerialMon.print(notif_tail);
        SerialMon.print(" => returning ");
        SerialMon.println(!notif_queue_empty ? "true" : "false");
    }
    return !notif_queue_empty;
}

void sendSerialToApp(const String& msg) {
    if (msg.length() == 0) return;
    SerialMon.print(msg);
    
    // Stall period: drop all serial notifications until SoftDevice stabilizes after connect.
    // Sending during handshake fills the notif queue with log lines that never drain,
    // causing massive fragmentation and duplicate NOTIF entries on the app side.
    if (millis() < ble_connect_stall_until) return;

    // Use the existing notification queue — it already handles wrapping, ~~ terminators, and drain timing
    static char notifStr[NOTIF_STR_MAX_LEN];
    snprintf(notifStr, sizeof(notifStr), "LINE:SERIAL|DATA:%s", msg.c_str());
    enqueue(notifStr);
}

void sendSerialToAppLn(const String& msg) {
    if (msg.length() == 0) return;
    String lnMsg = String(msg) + "\n";
    sendSerialToApp(lnMsg);
}

static void sendSerialFromQueue();  // forward decl
void handleBLE() {
    static uint8_t handle_cnt = 0;
    if (handle_cnt++ % 50 == 0 && ble_connected) {
        SerialMon.print(F("[BLE] handleBLE #"));
        SerialMon.print(handle_cnt);
        SerialMon.print(" stall=");
        SerialMon.print(millis() < ble_connect_stall_until ? 1 : 0);
        SerialMon.print(" stall_until=");
        SerialMon.print(ble_connect_stall_until);
        SerialMon.print(" now=");
        SerialMon.print(millis());
        SerialMon.print(" pending_disp=");
        SerialMon.print(pending_display_update ? 1 : 0);
        SerialMon.print(" pending_sync=");
        SerialMon.print(pending_screen_sync ? 1 : 0);
        SerialMon.print(" drain_fail=");
        SerialMon.print(drain_failed ? 1 : 0);
        SerialMon.println();
    }

    // Stall queue/drain during connection handshake — SoftDevice needs time to stabilize
    if (millis() < ble_connect_stall_until) {
        sendSerialFromQueue();
        return;
    }

    // Clear stall after first post-stall cycle so we don't re-stall on reconnect
    if (ble_connect_stall_until > 0) ble_connect_stall_until = 0;

    // Process deferred display update out of SoftDevice callback context
    if (pending_display_update) {
        pending_display_update = false;
        if (strcmp(current_mode, "PTT") == 0) {
            drawPttLayout();
        } else {
            updModeAndChannelDisplay();
        }
    }

    // Process deferred screen sync — was being dropped because sendNotificationToApp checks in_write_callback
    if (pending_screen_sync) {
        pending_screen_sync = false;
        
        // Call the real screen sync function instead of dummy data
        extern void sendScreenSync();
        sendScreenSync();
    }

    if (ble_connected) {
        SerialMon.print(F("[BLE] calling drainQueue: connected="));
        SerialMon.print(ble_connected ? 1 : 0);
        SerialMon.print(" empty=");
        SerialMon.print(notif_queue_empty ? 1 : 0);
        SerialMon.print(" head=");
        SerialMon.print(notif_head);
        SerialMon.print(" tail=");
        SerialMon.print(notif_tail);
        SerialMon.println();
        drainQueue();
    } else {
        static uint32_t last_summary = 0;
        if (ble_connected && millis() - last_summary > 2000) {
            last_summary = millis();
            uint8_t queue_count = (notif_head >= notif_tail) ? (notif_head - notif_tail) : (NOTIF_QUEUE_SIZE - notif_tail + notif_head);
            SerialMon.print(F("[BLE] summary: connected="));
            SerialMon.print(ble_connected ? 1 : 0);
            SerialMon.print(" empty=");
            SerialMon.print(notif_queue_empty ? 1 : 0);
            SerialMon.print(" head=");
            SerialMon.print(notif_head);
            SerialMon.print(" tail=");
            SerialMon.print(notif_tail);
            SerialMon.print(" count=");
            SerialMon.print(queue_count);
            SerialMon.print(" drain_failed=");
            SerialMon.print(drain_failed ? 1 : 0);
            SerialMon.println();
        }
    }
    sendSerialFromQueue();
    drainBinaryQueue();
}

bool isBleConnected() { return ble_connected; }

// sendSerialFromQueue is a no-op — serial lines go straight to the notification queue via enqueue()
static void sendSerialFromQueue() {
    // nothing: sendSerialToApp calls enqueue() directly above
}

void onConnect(uint16_t conn_handle) {
    SerialMon.print(F("[BLE] onConnect: setting ble_connected=true, stall_until="));
    SerialMon.println(millis() + BLE_CONNECT_STALL_MS);
    ble_connected = true;
    // Stall drain for 1.5s to let SoftDevice stabilize — prevents handshake deadlock
    ble_connect_stall_until = millis() + BLE_CONNECT_STALL_MS;
    sendSerialToAppLn("[BLE] connected");
}

void onDisconnect(uint16_t conn_handle, uint8_t reason) {
    SerialMon.print(F("[BLE] onDisconnect: clearing queue, head="));
    SerialMon.print(notif_head);
    SerialMon.print(" tail=");
    SerialMon.println(notif_tail);
    ble_connected = false;
    notif_queue_empty = true;
    notif_head = 0;
    notif_tail = 0;
    ble_connect_stall_until = 0;
    
    // Reset drain failure state — prevents stall from blocking first messages after reconnect
    drain_failed = false;
    drain_fail_start = 0;
    drain_stall_until = 0;
    sendSerialToAppLn("[BLE] disconnected");

    // Reset PTT states to prevent stale "SENDING" or "RECEIVING" indicators
    setPttTxActive(false);
    setPttRxActive(false);

    // Defer display update out of SoftDevice callback context
    pending_display_update = true;
}

void onCharacteristicWritten(uint16_t conn_handle, BLECharacteristic* chr, uint8_t* data, uint16_t len) {
    in_write_callback = true;

    // Copy bytes to local buffer FIRST — never iterate callback pointer through SoftDevice context
    char localBuf[256];
    int nlen = len < (int)(sizeof(localBuf) - 1) ? (int)len : (int)(sizeof(localBuf) - 1);
    for (int i = 0; i < nlen; i++) { localBuf[i] = (char)data[i]; }
    localBuf[nlen] = '\0';

    // Check printable on local buffer
    bool printable = true;
    for (int i = 0; i < nlen; i++) {
        uint8_t b = (uint8_t)localBuf[i];
        if (b >= 0xC2) continue;
        if (b >= 0x80 && b < 0xC2) continue;
        if (b < 0x20 && b != 0x09 && b != 0x0A && b != 0x0D) { printable = false; break; }
        if (b == 0x7F) { printable = false; break; }
        if (b >= 0xA0 && b < 0xC2) { printable = false; break; }
    }

    bool handled = false;
    if (printable) {
        int delim = -1;
        for (int i = 0; i < nlen; i++) { if (localBuf[i] == ':') { delim = i; break; } }
        if (delim != -1) {
            char action[64]; int alen = delim < sizeof(action)-1 ? delim : sizeof(action)-1;
            for (int i=0;i<alen;i++) action[i] = localBuf[i]; action[alen]='\0';
            char value[256]; int vstart=delim+1, vlen=nlen-vstart;
            vlen = vlen < sizeof(value)-1 ? vlen : sizeof(value)-1;
            for (int i=0;i<vlen;i++) value[i] = localBuf[vstart+i]; value[vlen]='\0';
            if (strcmp(action,"SETMODE")==0) { switchMode(String(value)); handled=true; }
            else if (strcmp(action,"SENDTXT")==0) { sendTxtMessage(value); handled=true; }
            else if (strcmp(action,"SETNAME")==0) { static char localName[32]; int slen=strlen(value); if(slen>0&&slen<sizeof(localName)){for(int i=0;i<slen;i++)localName[i]=value[i];localName[slen]='\0';buddySetDisplayName(localName);char r[48];snprintf(r,sizeof(r),"OK{NAME:%s}",localName);sendNotificationToApp(r);}else{sendNotificationToApp("ERR{NAME:too long}");handled=true;} }
            else if (strcmp(action,"SETBUDDY")==0) { int c=buddyImportCsv(value);char r[48];snprintf(r,sizeof(r),"OK{BUDDY:%d}",c);sendNotificationToApp(r);handled=true; }
            else if (strcmp(action,"GETBUDDY")==0) { static char cb[512];if(buddyExportCsv(cb,sizeof(cb))){char r[560];snprintf(r,sizeof(r),"OK{BUDDY:%s}",cb);sendNotificationToApp(r);}else{sendNotificationToApp("OK{BUDDY:}");}handled=true; }
            else if (strcmp(action,"GETSCREEN")==0) { int p=getPendingNotificationCount();if(p>3){char r[48];snprintf(r,sizeof(r),"OK{SCREEN:deferred:p=%d}",p);sendNotificationToApp(r);}else{pending_screen_sync=true;}handled=true; }
            else if (strcmp(action,"GETSTATUS")==0) { extern bool isPeerAlive();bool la=isPeerAlive();char r[32];snprintf(r,sizeof(r),"OK{BLE:1}{LORA:%d}",la?1:0);sendNotificationToApp(r);handled=true; }
            else if (strcmp(action,"GETSETTINGS")==0) { char buf[192];snprintf(buf,sizeof(buf),"OK{SETTINGS:SF=%d,BITRATE=%d,CHAN=%c,VOL=%d,BL=%d,BW=%d,CR=%d,FH=%d,HOUR=%d,MIN=%d,SEC=%d}",deviceSettings.spreading_factor,deviceSettings.bitrate_idx,channels[deviceSettings.channel_idx],deviceSettings.volume_level,deviceSettings.backlight?1:0,deviceSettings.bandwidth_idx,deviceSettings.coding_rate_idx,deviceSettings.frequency_hopping_enabled?1:0,deviceSettings.hours,deviceSettings.minutes,deviceSettings.seconds);sendNotificationToApp(buf);handled=true; }
            else if (strcmp(action,"SETSETTINGS")==0) { extern void setupLoRa();char buf[256];int vlen=strlen(value);if(vlen>=sizeof(buf))vlen=sizeof(buf)-1;for(int i=0;i<vlen;i++)buf[i]=value[i];buf[vlen]='\0';bool needReinit=false;char*cp=buf;while(cp&&*cp){char*kp=strchr(cp,',');int klen=kp?kp-cp:vlen;if(klen>=64)klen=63;char key[64],val[64];memcpy(key,cp,klen);key[klen]='\0';if(!kp)break;char*eqp=strchr(key,'=');if(!eqp){cp=kp+1;continue;*eqp='\0';strncpy(val,eqp+1,sizeof(val)-1);val[sizeof(val)-1]='\0';if(strcmp(key,"SF")==0){deviceSettings.spreading_factor=atoi(val);}else if(strcmp(key,"BITRATE")==0){deviceSettings.bitrate_idx=atoi(val);needReinit=true;}else if(strcmp(key,"CHAN")==0){char c=toupper(val[0]);deviceSettings.channel_idx=c-'A';needReinit=true;}else if(strcmp(key,"VOL")==0){deviceSettings.volume_level=atoi(val);}else if(strcmp(key,"BL")==0){deviceSettings.backlight=!!atoi(val);}else if(strcmp(key,"BW")==0){deviceSettings.bandwidth_idx=atoi(val);needReinit=true;}else if(strcmp(key,"CR")==0){deviceSettings.coding_rate_idx=atoi(val);needReinit=true;}else if(strcmp(key,"FH")==0){deviceSettings.frequency_hopping_enabled=!!atoi(val);}else if(strcmp(key,"HOUR")==0){deviceSettings.hours=atoi(val);}else if(strcmp(key,"MIN")==0){deviceSettings.minutes=atoi(val);}else if(strcmp(key,"SEC")==0){deviceSettings.seconds=atoi(val);}}cp=kp?kp+1:nullptr;}if(needReinit)setupLoRa();sendNotificationToApp("OK{SETTINGS:saved}");handled=true; }
            else { handled=true; }
        } else {
            if (nlen==9 && localBuf[0]=='G' && localBuf[1]=='E' && localBuf[2]=='T' && localBuf[3]=='S' && localBuf[4]=='T' && localBuf[5]=='A' && localBuf[6]=='T' && localBuf[7]=='U' && localBuf[8]=='S') { extern bool isPeerAlive();bool la=isPeerAlive();char r[32];snprintf(r,sizeof(r),"OK{BLE:1}{LORA:%d}",la?1:0);sendNotificationToApp(r);handled=true; }
            else if (nlen==9 && localBuf[0]=='G' && localBuf[1]=='E' && localBuf[2]=='T' && localBuf[3]=='S' && localBuf[4]=='C' && localBuf[5]=='R' && localBuf[6]=='E' && localBuf[7]=='E' && localBuf[8]=='N') { int p=getPendingNotificationCount();if(p>3){char r[48];snprintf(r,sizeof(r),"OK{SCREEN:deferred:p=%d}",p);sendNotificationToApp(r);}else{pending_screen_sync=true;}handled=true; }
            else { handled=true; }
        }
    }

    if (!handled && nlen>=4 && (uint8_t)localBuf[0]==0xFE && (uint8_t)localBuf[1]==0x01) {
        uint16_t opusLen = ((uint16_t)(uint8_t)localBuf[3] << 8) | (uint8_t)localBuf[2];
        if(opusLen>0 && 4+opusLen<=nlen){extern void sendPacket(uint8_t*,uint16_t,unsigned int);static char opusPkt[MAX_PKT];snprintf(opusPkt,sizeof(opusPkt),"PT%cO",channels[deviceSettings.channel_idx]);int pl=strlen(opusPkt);for(int i=0;i<opusLen&&pl+i<MAX_PKT-1;i++)opusPkt[pl+i]=localBuf[4+i];pl+=opusLen;setPttTxActive(true);pending_display_update=true;sendPacket((uint8_t*)opusPkt,(uint16_t)pl,0);}
    }

    in_write_callback = false;
}

// Function to send a notification to the app — queues for deferred delivery
void sendNotificationToApp(const char* message) {
    if (!message || !message[0]) return;

    // Stall period: drop all notifications until SoftDevice stabilizes after connect
    if (millis() < ble_connect_stall_until) {
        SerialMon.println(F("DBG:sendNotif DROPPED stall"));
        return;
    }

    // In write callback: enqueue for later drain, don't drop.
    // The queue will be drained in handleBLE() after the callback returns.
    if (in_write_callback) {
        size_t msgLen = strlen(message);
        static char notifStr_wc[NOTIF_STR_MAX_LEN];
        uint8_t buffer_wc[msgLen + 3];
        memcpy(buffer_wc, message, msgLen);
        buffer_wc[msgLen] = '~';
        buffer_wc[msgLen + 1] = '~';
        buffer_wc[msgLen + 2] = '\0';
        snprintf(notifStr_wc, sizeof(notifStr_wc), "LINE:NOTIF|DATA:%.*s", (int)(msgLen + 2), buffer_wc);
        
        // Use fragmentation when the wrapped message exceeds MTU
        if (strlen(notifStr_wc) > MAX_BLE_MTU) {
            extern bool sendFragmentedNotification(const char*);
            sendFragmentedNotification(message);
        }
        
        if (!enqueue(notifStr_wc)) {
            return; // queue full, nothing we can do in write callback anyway
        }
        // Clear stall state so handleBLE() will drain after this callback returns
        ble_connect_stall_until = 0;
        drain_failed = false;
        drain_stall_until = 0;
        if (drain_fail_start > 0) drain_fail_start = 0;
        return;
    }

    size_t msgLen = strlen(message);
    
    // Check if the wrapped message would exceed MTU — use fragmentation to avoid truncation
    extern bool sendFragmentedNotification(const char*);
    if (msgLen + 2 > MAX_BLE_MTU) {
        sendFragmentedNotification(message);
    }
    
    // Create buffer with "~~" terminator
    uint8_t buffer[msgLen + 3];
    memcpy(buffer, message, msgLen);
    buffer[msgLen] = '~';
    buffer[msgLen + 1] = '~';
    buffer[msgLen + 2] = '\0';

    // Build notification string for queue — fits within MTU after this check
    static char notifStr[NOTIF_STR_MAX_LEN];
    snprintf(notifStr, sizeof(notifStr), "LINE:NOTIF|DATA:%.*s", (int)(msgLen + 2), buffer);
    
    if (!enqueue(notifStr)) {
        // Queue full — drop notification rather than deadlocking the BLE stack.
        // Drain will happen next iteration of loop() via handleBLE().
    }
}

// Helper function to check if the data contains printable characters.
// Accepts UTF-8 multi-byte sequences (bytes with high bit set) while rejecting
// control characters (0x00-0x1F except 0x09 TAB, 0x0A LF, 0x0D CR).
bool isDataPrintable(const uint8_t* data, int length) {
    for (int i = 0; i < length; i++) {
        uint8_t b = data[i];
        // UTF-8 continuation bytes (10xxxxxx) and lead bytes (11xxxxxx) are valid
        if (b >= 0xC2) continue;   // Lead byte for 2+,3+ byte UTF-8 sequences
        if (b >= 0x80 && b < 0xC2) continue;  // Continuation byte 10xxxxxx
        // Reject control chars 0x00-0x1F except TAB, LF, CR
        if (b < 0x20 && b != 0x09 && b != 0x0A && b != 0x0D) return false;
        // DEL (0x7F) and bytes 0xA0-0xBF alone (invalid UTF-8 continuation) are bad
        if (b == 0x7F) return false;
        if (b >= 0xA0 && b < 0xC2) return false;
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
