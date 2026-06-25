document.addEventListener('deviceready', onDeviceReady, false);

function onDeviceReady() {
    logMessage('Cordova is ready.');
    updateDeviceStatus('Device ready, scanning for BLE devices...');
    setStatusIndicator('scanning');
    app.initialize();
}

// Add message to console
function logMessage(message) {
    console.log(message);
    const consoleDiv = document.getElementById('console');
    const newMessage = document.createElement('p');
    newMessage.textContent = message;
    consoleDiv.appendChild(newMessage);
    consoleDiv.scrollTop = consoleDiv.scrollHeight;
}

// Function to update the device status
function updateDeviceStatus(status) {
    document.getElementById('deviceStatus').textContent = status;
}

// Update status indicator dot color class
function setStatusIndicator(state) {
    const dot = document.getElementById('statusDot');
    dot.className = 'status-indicator ' + state;
}

// Function to update device information display
function updateDeviceInfo(info) {
    document.getElementById('deviceInfo').textContent = info;
    document.getElementById('infoSection').style.display = 'block';
}

function switchMode(aMode) {
	app.sendDataToDevice("SETMODE:"+aMode);
	showToast(aMode + ' mode');
	// Update active pill
	document.querySelectorAll('.modePill').forEach(btn => {
		btn.classList.toggle('active', btn.getAttribute('data-mode') === aMode);
	});
}

// Function to send data to device
function sendData() {
    const inputField = document.getElementById('inputField');
    const data = inputField.value;
    if (data) {
        logMessage('Sending: ' + data);
        app.sendDataToDevice("SENDTXT:"+data);
        inputField.value = '';
    } else {
        logMessage('Please enter some text to send.');
    }
}

// Toast notification helper
let toastTimer = null;
function showToast(message) {
	const toast = document.getElementById('toast');
	toast.textContent = message;
	toast.classList.add('visible');
	if (toastTimer) clearTimeout(toastTimer);
	toastTimer = setTimeout(() => {
		toast.classList.remove('visible');
	}, 1500);
}

// Log panel toggle helpers
function openLogPanel() { document.getElementById('logPanel').classList.add('open'); }
function closeLogPanel() { document.getElementById('logPanel').classList.remove('open'); }

var app = {
    connectedDeviceId: null,
    reconnectDelay: 3000,
    isDeviceConnected: false,
    serviceUUID: "1235",
    characteristicUUID: "ABCE",
	notificationBuffer: "",
    initialize: function() {
        this.bindEvents();
        const sendButton = document.getElementById('sendForm');
        sendButton.addEventListener('submit', function(e) {
            e.preventDefault();
            sendData();
        });
		const modeButtons = document.querySelectorAll('.modePill');
		modeButtons.forEach(button => {
			button.addEventListener('click', function() {
				const mode = this.getAttribute('data-mode');
				switchMode(mode);
			});
		});
        // Log panel toggles
        document.getElementById('logToggle').addEventListener('click', function() {
            openLogPanel();
        });
        document.getElementById('logClose').addEventListener('click', function() {
            closeLogPanel();
        });
    },
    bindEvents: function() {
        document.addEventListener('deviceready', this.onDeviceReady.bind(this), false);
    },
    onDeviceReady: function() {
        this.scanForDevice();
    },
    scanForDevice: function() {
        if (this.isDeviceConnected) {
            return;
        }
		else {
			setTimeout(function() {
				app.scanForDevice();
			}, app.reconnectDelay);
		}

        logMessage("Scanning for BLE devices...");
        ble.scan([], 10, function(device) {
            if (device.name && app.isValidDeviceName(device.name)) {
                logMessage("Device found: " + device.name);
                updateDeviceStatus("Device found, connecting...");
                setStatusIndicator('scanning');
                app.connectToDevice(device.id);
            }
        }, function(error) {
            logMessage("Error scanning: " + error);
            updateDeviceStatus("Error scanning for devices.");
            setStatusIndicator('disconnected');
            setTimeout(function() {
                logMessage("Re-attempting to scan...");
                app.scanForDevice();
            }, app.reconnectDelay);
        });
    },
    isValidDeviceName: function(deviceName) {
        const pattern = /^LilygoT-Echo-[A-F0-9]{8}$/;
        return pattern.test(deviceName);
    },
    connectToDevice: function(deviceId) {
		if(app.isDeviceConnected) {
			logMessage("Already connected. Ignoring: " + deviceId);
			return;
		}
		app.isDeviceConnected = true;
        logMessage("Attempting to connect to device with ID: " + deviceId);
        ble.connect(deviceId, function(peripheral) {
            if (peripheral && peripheral.id) {
                logMessage("Successfully connected to " + peripheral.name);
                logMessage("Device ID: " + peripheral.id);
                updateDeviceStatus("Connected to " + peripheral.name);
                setStatusIndicator('connected');

                app.connectedDeviceId = peripheral.id;
                app.isDeviceConnected = true;

                app.readWriteBLE(peripheral.id);
                app.startNotification(peripheral.id);
            } else {
                logMessage("Connected, but peripheral.id is not available.");
				app.isDeviceConnected = false;
            }
        }, function(error) {
            logMessage("Error connecting: " + error);
            updateDeviceStatus("Error connecting to device.");
            setStatusIndicator('disconnected');
			app.isDeviceConnected = false;
            setTimeout(function() {
                logMessage("Re-attempting to connect...");
                app.scanForDevice();
            }, app.reconnectDelay);
        });
    },
    readWriteBLE: function(deviceId) {
        ble.read(deviceId, app.serviceUUID, app.characteristicUUID, function(data) {
            var byteArray = new Uint8Array(data);
            var receivedValue = app.bytesToString(byteArray);
        }, function(error) {
            logMessage("Error reading: " + error);
        });

        ble.isConnected(deviceId, function(connected) {
            if (!connected) {
                logMessage("Device disconnected.");
                updateDeviceStatus("Device disconnected.");
                setStatusIndicator('disconnected');
                app.isDeviceConnected = false;
                setTimeout(function() {
                    app.scanForDevice();
                }, app.reconnectDelay);
            }
        });
    },
    sendDataToDevice: function(data) {
        if (!app.connectedDeviceId) {
            logMessage("No device connected to send data.");
            return;
        }
        var bytes = app.stringToBytes(data);
        ble.write(app.connectedDeviceId, app.serviceUUID, app.characteristicUUID, bytes, function() {
            logMessage("Data written: " + data);
        }, function(error) {
            logMessage("Error writing data: " + error);
        });
    },
	startNotification: function(deviceId) {
		logMessage("Starting notifications from device...");
		app.notificationBuffer = '';

		ble.startNotification(deviceId, app.serviceUUID, app.characteristicUUID, function(data) {
			var byteArray = new Uint8Array(data);
			var receivedNotification = app.bytesToString(byteArray);

			app.notificationBuffer += receivedNotification;

			var endMarkerIndex = app.notificationBuffer.indexOf("~~");

			while (endMarkerIndex !== -1) {
				var completeMessage = app.notificationBuffer.slice(0, endMarkerIndex);
				app.processCompleteMessage(completeMessage);
				app.notificationBuffer = app.notificationBuffer.slice(endMarkerIndex + 2);
				endMarkerIndex = app.notificationBuffer.indexOf("~~");
			}
		}, function(error) {
			logMessage("Error receiving notification: " + error);
		});
	},
	processCompleteMessage: function(message) {
		var lineRegex = /LINE:(\d+)/;
		var textRegex = /TEXT:(.*)/;

		var lineMatch = message.match(lineRegex);
		var textMatch = message.match(textRegex);

		if (lineMatch && textMatch) {
			var lineNumber = parseInt(lineMatch[1]+lineMatch[2]);
			var text = textMatch ? textMatch[1] : '';
			if(lineNumber<11) {
				document.getElementById('line' + lineNumber).textContent = text;
				document.getElementById('screen').classList.remove('empty-hint');
			}
		} else {
			updateDeviceInfo(message);
			logMessage("Received message: " + message);
		}
	},
    stringToBytesUtf16: function(string) {
        var array = new Uint8Array(string.length);
        for (var i = 0, l = string.length; i < l; i++) {
            array[i] = string.charCodeAt(i);
        }
        return array.buffer;
    },
	stringToBytes: function(string) {
		var utf8Encoder = new TextEncoder();
		return utf8Encoder.encode(string).buffer;
	},
    bytesToString: function(byteArray) {
		var result = "";
		for (var i = 0; i < byteArray.length; i++) {
			if (i < byteArray.length - 1 && byteArray[i] === 126 && byteArray[i + 1] === 126) {
				result += String.fromCharCode(byteArray[i]);
				result += String.fromCharCode(byteArray[i + 1]);
				break;
			}
			if (byteArray[i] === 0) {
				break;
			}
			result += String.fromCharCode(byteArray[i]);
		}
		return result;
	}
};
