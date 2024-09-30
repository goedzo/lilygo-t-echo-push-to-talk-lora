document.addEventListener('deviceready', onDeviceReady, false);

function onDeviceReady() {
    logMessage('Cordova is ready.');
    updateDeviceStatus('Device ready, scanning for BLE devices...');
    app.initialize();  // This now handles everything, including button event listeners
}

// Add message to console
function logMessage(message) {
	console.log(message);
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
        
        // Use the correct BLE write function from the app object
        app.sendDataToDevice(data);  // New function to send the data to BLE device

        inputField.value = ''; // Clear input after sending
    } else {
        logMessage('Please enter some text to send.');
    }
}

var app = {
    connectedDeviceId: null,  // Store the connected device ID
    reconnectDelay: 5000,     // Delay before attempting to reconnect (5 seconds)

    initialize: function() {
        this.bindEvents();
        const sendButton = document.getElementById('sendButton');
        sendButton.addEventListener('click', sendData);
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
            updateDeviceStatus("Error scanning for devices.");
        });
    },
    isValidDeviceName: function(deviceName) {
        const pattern = /^LilygoT-Echo-[A-F0-9]{8}$/; // Matches LilygoT-Echo-XXXXXXXX (8 hex characters)
        return pattern.test(deviceName);
    },
    connectToDevice: function(deviceId) {
        logMessage("Attempting to connect to device with ID: " + deviceId);
        ble.connect(deviceId, function(peripheral) {
            // Verify if we are getting the peripheral object and deviceId
            if (peripheral && peripheral.id) {
                logMessage("Successfully connected to " + peripheral.name);
                logMessage("Device ID: " + peripheral.id);
                updateDeviceStatus("Connected to " + peripheral.name);

                // Store the connected device ID
                app.connectedDeviceId = peripheral.id;

                app.readWriteBLE(peripheral.id);
            } else {
                logMessage("Connected, but peripheral.id is not available.");
            }
        }, function(error) {
            logMessage("Error connecting: " + error);
            updateDeviceStatus("Error connecting to device.");

            // Re-attempt connection after a delay
            setTimeout(function() {
                logMessage("Re-attempting to connect...");
                app.scanForDevice();
            }, app.reconnectDelay);
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

        // Monitor disconnection and automatically restart scanning
        ble.isConnected(deviceId, function(connected) {
            if (!connected) {
                logMessage("Device disconnected.");
                updateDeviceStatus("Device disconnected.");

                // Automatically start scanning again after a delay
                setTimeout(function() {
                    app.scanForDevice();
                }, app.reconnectDelay);
            }
        });
    },
    // Function to send data to the connected device
    sendDataToDevice: function(data) {
        if (!app.connectedDeviceId) {
            logMessage("No device connected to send data.");
            return;
        }

        var serviceUUID = "1234";
        var characteristicUUID = "ABCD";
        var bytes = app.stringToBytes(data);

        // Use the connected device ID for writing data
        ble.write(app.connectedDeviceId, serviceUUID, characteristicUUID, bytes, function() {
            logMessage("Data written: " + data);
        }, function(error) {
            logMessage("Error writing data: " + error);
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
