document.addEventListener('deviceready', onDeviceReady, false);

function onDeviceReady() {
    logMessage('Cordova is ready.');
    updateDeviceStatus('Device ready, scanning for BLE devices...');
    app.initialize();
	initializeApp();
}

// Add message to console
function logMessage(message) {
    const consoleDiv = document.getElementById('console');
    const newMessage = document.createElement('p');
    newMessage.textContent = message;
    consoleDiv.appendChild(newMessage);
    consoleDiv.scrollTop = consoleDiv.scrollHeight; // Auto-scroll to the bottom
}

// Function to update the device status
function updateDeviceStatus(status) {
    document.getElementById('deviceStatus').textContent = status;
}

// Function to update device information display
function updateDeviceInfo(info) {
    document.getElementById('deviceInfo').textContent = info;
}

// Function to send data to device
function sendData() {
    const inputField = document.getElementById('inputField');
    const data = inputField.value;
    if (data) {
        logMessage('Sending: ' + data);
        // Call the BLE write function here (replace with actual function from your BLE logic)
        app.writeToBLEDevice(data);
        inputField.value = ''; // Clear input after sending
    } else {
        logMessage('Please enter some text to send.');
    }
}

// Initialize the app: setting event listeners, and any other startup logic
function initializeApp() {
    const sendButton = document.getElementById('sendButton');
    sendButton.addEventListener('click', sendData);
}

var app = {
    initialize: function() {
        this.bindEvents();
    },
    bindEvents: function() {
        document.addEventListener('deviceready', this.onDeviceReady.bind(this), false);
    },
    onDeviceReady: function() {
        this.scanForDevice();
    },
    scanForDevice: function() {
        logMessage("Scanning for BLE devices...");
        ble.scan([], 10, function(device) {
            if (device.name && app.isValidDeviceName(device.name)) {
                logMessage("Device found: " + device.name);
                updateDeviceStatus("Device found, connecting...");
                app.connectToDevice(device.id);
            }
        }, function(error) {
            logMessage("Error scanning: " + error);
        });
    },
    isValidDeviceName: function(deviceName) {
        const pattern = /^LilygoT-Echo-[A-F0-9]{8}$/; // Matches LilygoT-Echo-XXXXXXXX (8 hex characters)
        return pattern.test(deviceName);
    },
    connectToDevice: function(deviceId) {
        ble.connect(deviceId, function(peripheral) {
            logMessage("Connected to " + peripheral.name);
            updateDeviceStatus("Connected to " + peripheral.name);
            app.readWriteBLE(peripheral.id);
        }, function(error) {
            logMessage("Error connecting: " + error);
            updateDeviceStatus("Error connecting to device.");
        });
    },
    readWriteBLE: function(deviceId) {
        var serviceUUID = "1234";
        var characteristicUUID = "ABCD";

        ble.read(deviceId, serviceUUID, characteristicUUID, function(data) {
            var receivedValue = app.bytesToString(data);
            logMessage("Received: " + receivedValue);
            updateDeviceInfo(receivedValue);  // Show received data in the UI
        }, function(error) {
            logMessage("Error reading: " + error);
        });

        var data = "Hello Device!";
        var bytes = app.stringToBytes(data);
        ble.write(deviceId, serviceUUID, characteristicUUID, bytes, function() {
            logMessage("Data written");
        }, function(error) {
            logMessage("Error writing: " + error);
        });
    },
    stringToBytes: function(string) {
        var array = new Uint8Array(string.length);
        for (var i = 0, l = string.length; i < l; i++) {
            array[i] = string.charCodeAt(i);
        }
        return array.buffer;
    },
    bytesToString: function(buffer) {
        return String.fromCharCode.apply(null, new Uint8Array(buffer));
    }
};

app.initialize();
