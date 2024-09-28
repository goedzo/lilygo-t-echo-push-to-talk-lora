document.addEventListener('deviceready', onDeviceReady, false);

function onDeviceReady() {
    logMessage('Cordova is ready.');
    updateDeviceStatus('Device ready, scanning for BLE devices...');
    app.initialize();
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
