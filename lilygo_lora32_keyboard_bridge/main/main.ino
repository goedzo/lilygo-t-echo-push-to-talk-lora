#include <utilities.h>
#include <bluefruit.h>
#include <HID-Project.h>  // Include HID-Project Library

#define CONNECTION_TIMEOUT_MS 5000  // 5 seconds timeout for connection attempts
#define heartbeat_TIMEOUT_MS 5000  //

BLEClientDis disClient; // GATT client for Device Information Service (DIS)

uint32_t lastConnectionAttempt = 0;  // Time of the last connection attempt
uint32_t heartbeat = 0;  // Time of the last connection attempt
bool isConnecting = false;           // Flag to track if we are attempting to connect
uint16_t connHandle = BLE_CONN_HANDLE_INVALID;  // Connection handle to track the current connection

// List to store MAC addresses of devices that have been tried (as strings)
const int MAX_TRIED_DEVICES = 50;  // Allow more devices to be tried
String triedDevices[MAX_TRIED_DEVICES];
int triedDeviceCount = 0;

String currentDeviceAddr;  // Store current device address during connection attempt

// Helper function to convert a MAC address to a string
String addressToString(uint8_t* addr) {
  char str[18];  // MAC address as string "XX:XX:XX:XX:XX:XX"
  snprintf(str, sizeof(str), "%02X:%02X:%02X:%02X:%02X:%02X",
           addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
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

// Callback function when a connection is established
void connect_callback(uint16_t conn_handle) {
  BLEConnection* conn = Bluefruit.Connection(conn_handle);

  isConnecting = false;
  connHandle = conn_handle;  // Store the connection handle

  // Discover GATT Services (including Device Information Service)
  if (disClient.discover(conn_handle)) {
    char buffer[32] = {0};  // Buffer to store characteristic values

    // Query the manufacturer name
    if (disClient.getManufacturer(buffer, sizeof(buffer))) {
      Serial.print("Manufacturer: ");
      Serial.println(buffer);
    }

    // Query the model number
    if (disClient.getModel(buffer, sizeof(buffer))) {
      Serial.print("Model: ");
      Serial.println(buffer);
    }

    // Query the serial number
    if (disClient.getSerial(buffer, sizeof(buffer))) {
      Serial.print("Serial Number: ");
      Serial.println(buffer);
    }

    // Query the firmware revision
    if (disClient.getFirmwareRev(buffer, sizeof(buffer))) {
      Serial.print("Firmware Version: ");
      Serial.println(buffer);
    }

    // Query the hardware revision
    if (disClient.getHardwareRev(buffer, sizeof(buffer))) {
      Serial.print("Hardware Version: ");
      Serial.println(buffer);
    }

    // Query the software revision (if available)
    if (disClient.getSoftwareRev(buffer, sizeof(buffer))) {
      Serial.print("Software Version: ");
      Serial.println(buffer);
    }

  } else {
    Serial.println("Device Information Service NOT found");
  }

  // If the device is a keyboard, initiate HID communication
  if (Bluefruit.Scanner.checkReportForUuid(conn->getAdvReport(), 0x1812)) {
    Serial.println("BLE Keyboard detected. Sending test key...");
    Keyboard.begin();  // Start the HID Keyboard

    // Send a test key (e.g., the letter 'A')
    Keyboard.print("A");
    Keyboard.end();
  }

  // Disconnect after retrieving the information
  conn->disconnect();
}

// Callback function when a connection is dropped or disconnected
void disconnect_callback(uint16_t conn_handle, uint8_t reason) {
  connHandle = BLE_CONN_HANDLE_INVALID;  // Reset the connection handle
  isConnecting = false;
  // Scanning will automatically resume due to `restartOnDisconnect`
}

void scan_callback(ble_gap_evt_adv_report_t* report) {
  // Stop scanning before connecting
  Bluefruit.Scanner.stop();

  // Convert the device address to a string
  currentDeviceAddr = addressToString(report->peer_addr.addr);

  // Check if the device is in the tried list before proceeding
  if (isDeviceInTriedList(currentDeviceAddr)) {
    Bluefruit.Scanner.start(0);  // Resume scanning if the device has already been tried
    return;  // Exit if the device is in the tried list
  }

  // Add the device to the tried list
  addDeviceToTriedList(currentDeviceAddr);

  // Print new device found
  Serial.printf("New Device: %s\n", currentDeviceAddr.c_str());

  // Compact Advertising Report Type Details
  Serial.printf("Connectable: %d, Scannable: %d, Directed: %d, Scan Resp: %d, Ext PDU: %d, Data Status: %d\n",
                report->type.connectable, report->type.scannable, report->type.directed,
                report->type.scan_response, report->type.extended_pdu, report->type.status);

  // Skip non-connectable devices
  if (report->type.connectable == 0) {
    Serial
