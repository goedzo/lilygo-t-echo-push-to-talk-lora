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
    reconnectDelay: 3000,     // Delay before attempting to reconnect (3 seconds)
    isDeviceConnected: false,  // Flag to track the connection status
    serviceUUID: "1235",
    characteristicUUID: "ABCE",
	notificationBuffer: "",
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
        if (this.isDeviceConnected) {
            // If already connected, don't scan
            //logMessage("Device is already connected. No need to scan.");
            return;
        }
		else {
			setTimeout(function() {
				//logMessage("Re-attempting to scan...");
				app.scanForDevice();
			}, app.reconnectDelay);
		}

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
            
            // Restart scanning if no device was found or if there's an error
            setTimeout(function() {
                logMessage("Re-attempting to scan...");
                app.scanForDevice();
            }, app.reconnectDelay);
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

                // Store the connected device ID and set the flag
                app.connectedDeviceId = peripheral.id;
                app.isDeviceConnected = true;

                app.readWriteBLE(peripheral.id);  // Set up reading and writing
                app.startNotification(peripheral.id);  // Start listening for notifications
            } else {
                logMessage("Connected, but peripheral.id is not available.");
            }
        }, function(error) {
            logMessage("Error connecting: " + error);
            updateDeviceStatus("Error connecting to device.");
            // Re-attempt connection after a delay
			app.isDeviceConnected = false;
            setTimeout(function() {
                logMessage("Re-attempting to connect...");
                app.scanForDevice();  // Restart scanning if connection fails
            }, app.reconnectDelay);
        });
    },
    readWriteBLE: function(deviceId) {
        ble.read(deviceId, app.serviceUUID, app.characteristicUUID, function(data) {
            var byteArray = new Uint8Array(data);  // Convert to Uint8Array
            var receivedValue = app.bytesToString(byteArray);  // Convert only the valid part
            //logMessage("Received: " + receivedValue);
            //updateDeviceInfo(receivedValue);  // Show received data in the UI
        }, function(error) {
            logMessage("Error reading: " + error);
        });

        // Monitor disconnection and automatically restart scanning
        ble.isConnected(deviceId, function(connected) {
            if (!connected) {
                logMessage("Device disconnected.");
                updateDeviceStatus("Device disconnected.");
                
                // Reset the connection status and restart scanning
                app.isDeviceConnected = false;
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
        var bytes = app.stringToBytes(data);

        // Use the connected device ID for writing data
        ble.write(app.connectedDeviceId, app.serviceUUID, app.characteristicUUID, bytes, function() {
            logMessage("Data written: " + data);
        }, function(error) {
            logMessage("Error writing data: " + error);
        });
    },
	// This is called when we receive a BLE message (notification) from the device
	startNotification: function(deviceId) {
		logMessage("Starting notifications from device...");
		app.notificationBuffer = '';  // Buffer to accumulate message chunks

		ble.startNotification(deviceId, app.serviceUUID, app.characteristicUUID, function(data) {
			var byteArray = new Uint8Array(data);
			var receivedNotification = app.bytesToString(byteArray);
			//logMessage("Notification chunk received: " + receivedNotification);

			// Accumulate the received chunks in the buffer
			app.notificationBuffer += receivedNotification;

			// Check if the buffer contains the end marker "~~"
			var endMarkerIndex = app.notificationBuffer.indexOf("~~");

			// If the end marker "~~" is found, extract and process the complete message
			while (endMarkerIndex !== -1) {
				// Extract the complete message up to the end marker "~~"
				var completeMessage = app.notificationBuffer.slice(0, endMarkerIndex);

				//logMessage("Complete message received: " + completeMessage);

				// Process the complete message
				app.processCompleteMessage(completeMessage);

				// Remove the processed message and "~~" marker from the buffer
				app.notificationBuffer = app.notificationBuffer.slice(endMarkerIndex + 2);

				// Check again if there is another complete message in the buffer
				endMarkerIndex = app.notificationBuffer.indexOf("~~");
			}
		}, function(error) {
			logMessage("Error receiving notification: " + error);
		});
	},
	processCompleteMessage: function(message) {
		// Process the complete message
		//logMessage("Processing complete message: " + message);
		
		var lineRegex = /LINE:(\d+)/;
		var textRegex = /TEXT:(.*)/;

		var lineMatch = message.match(lineRegex);
		var textMatch = message.match(textRegex);

		if (lineMatch && textMatch) {
			var lineNumber = parseInt(lineMatch[1]+lineMatch[2]);
			var text = textMatch ? textMatch[1] : '';  // Handle the case where TEXT: is empty
			//Max lines = 10
			if(lineNumber<11) {
				document.getElementById('line' + lineNumber).textContent = text;
			}
		} else {
			//Normal device message
			updateDeviceInfo(message);
			logMessage("Received message: " + message);
		}
	},
    stringToBytes: function(string) {
        var array = new Uint8Array(string.length);
        for (var i = 0, l = string.length; i < l; i++) {
            array[i] = string.charCodeAt(i);
        }
        return array.buffer;
    },
    bytesToString: function(byteArray) {
		var result = "";
		for (var i = 0; i < byteArray.length; i++) {
			// Stop if we encounter the "~~" marker or null terminator
			if (i < byteArray.length - 1 && byteArray[i] === 126 && byteArray[i + 1] === 126) {  // Check for "~~"
				// Process the message up to "~~"
				result += String.fromCharCode(byteArray[i]);  // Add the first "~"
				result += String.fromCharCode(byteArray[i + 1]);  // Add the second "~"
				break;  // Stop reading further
			}
			if (byteArray[i] === 0) {
				// Stop if we encounter a null terminator
				break;
			}
			result += String.fromCharCode(byteArray[i]);
		}
		return result;
	}
};

