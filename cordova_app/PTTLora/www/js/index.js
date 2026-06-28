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

// Message history storage (sent and received)
const messageHistory = [];
const MAX_HISTORY = 100;

function addMessageToHistory(type, text, timestamp, status) {
    const msg = { type: type, text: text, time: timestamp, status: status || 'pending' };
    messageHistory.push(msg);
    if (messageHistory.length > MAX_HISTORY) {
        messageHistory.shift();
    }
    updateMessageHistoryUI();
}

function updateMessageHistoryUI() {
    const container = document.getElementById('messageHistoryContainer');
    if (!container) return;
    
    // Group by type: received first, then sent
    let html = '';
    for (let i = 0; i < messageHistory.length; i++) {
        const m = messageHistory[i];
        const isReceived = m.type === 'received';
        const cls = isReceived ? 'history-msg received' : 'history-msg sent';
        const statusIcon = !isReceived ? getStatusIcon(m.status) : '';
        
        html += '<div class="' + cls + '">';
        if (isReceived) {
            html += '<div class="history-msg-time">' + m.time + '</div>';
            html += '<div class="history-msg-text">' + escapeHtml(m.text) + '</div>';
        } else {
            html += '<div class="history-msg-text">' + escapeHtml(m.text) + '</div>';
            const devTag = m.deviceName ? '[' + m.deviceName + '] ' : '';
            html += '<div class="history-msg-status">' + statusIcon + ' ' + devTag + m.status + '</div>';
        }
        html += '</div>';
    }
    container.innerHTML = html;
    
    // Scroll to bottom for received messages
    if (messageHistory.length > 0) {
        const lastItem = container.lastElementChild;
        if (lastItem && messageHistory[messageHistory.length-1].type === 'received') {
            lastItem.scrollIntoView({ behavior: 'smooth' });
        }
    }
    
    // Update badge count
    const badge = document.getElementById('historyBadge');
    if (badge) {
        const unreadCount = messageHistory.filter(m => m.type === 'received' && m.unread).length;
        if (unreadCount > 0) {
            badge.textContent = unreadCount > 99 ? '99+' : unreadCount;
            badge.style.display = 'inline-block';
        } else {
            badge.style.display = 'none';
        }
    }
}

function getStatusIcon(status) {
    switch (status) {
        case 'sent': return '&#10003;';
        case 'failed': return '&#10007;';
        case 'pending': return '&#9203;';
        default: return '';
    }
}

function escapeHtml(text) {
    var div = document.createElement('div');
    div.appendChild(document.createTextNode(text));
    return div.innerHTML;
}

// Toggle message history panel
function toggleMessageHistory() {
    var panel = document.getElementById('messageHistoryPanel');
    if (panel) {
        panel.classList.toggle('open');
    }
}

function clearMessageHistory() {
    messageHistory.length = 0;
    updateMessageHistoryUI();
    showToast('History cleared');
}

function switchMode(aMode) {
	app.currentMode = aMode;
	var targetDeviceName = app.targetDeviceName;
	if (targetDeviceName) {
		app.sendDataToDevice(targetDeviceName, "SETMODE:"+aMode);
	} else {
		app.sendDataToDeviceToAll("SETMODE:"+aMode);
		showToast(aMode + ' mode');
		return;
	}
	showToast(aMode + ' mode');
	// Update active pill
	document.querySelectorAll('.modePill').forEach(btn => {
		btn.classList.toggle('active', btn.getAttribute('data-mode') === aMode);
	});
	
	// Toggle PTT section visibility
	var inputSection = document.getElementById('inputSection');
	var pttSection = document.getElementById('pttSection');
	if (aMode === 'PTT') {
		inputSection.style.display = 'none';
		pttSection.style.display = 'block';
		app.startPttStatusPolling();
	} else if (aMode === 'TXT') {
		inputSection.style.display = 'block';
		pttSection.style.display = 'none';
		app.startTxtStatusPolling(targetDeviceName);
	} else if (aMode === 'WP') {
		inputSection.style.display = 'none';
		pttSection.style.display = 'none';
		document.getElementById('mapSection').style.display = 'block';
		app.stopPttStatusPolling();
		app.stopTxtStatusPolling();
		app.renderWaypointMap();
	} else {
		inputSection.style.display = 'block';
		pttSection.style.display = 'none';
		app.stopPttStatusPolling();
		app.stopTxtStatusPolling();
	}
}

// Function to send data to device
function sendData() {
    const inputField = document.getElementById('inputField');
    const data = inputField.value;
    if (data) {
        // Check character limit before sending
        if (data.length > app.TXT_MAX_CHARS) {
            logMessage('Message too long: ' + data.length + '/' + app.TXT_MAX_CHARS);
            showToast('Too long (' + data.length + '/' + app.TXT_MAX_CHARS + ')');
            return;
        }
        
        // Start send state machine
        app.setTxtSendState('sending');
        
        logMessage('Sending: ' + data);
        app.sendDataToDevice(app.targetDeviceName, "SENDTXT:"+data, function(success) {
            if (success) {
                var devName = app.getShortDeviceId(app.targetDeviceName) || app.targetDeviceName;
                addMessageToHistory('sent', data, formatTimestamp(), 'pending');
                // After send succeeds over BLE, show "Sent" and wait for LoRa confirm
                setTimeout(function() {
                    // Show "Transmitting..." while waiting for device confirmation
                    setTxtSendState('transmitting');
                    
                    // Set a timeout for ACK (30 seconds)
                    app.txtAckTimer = setTimeout(function() {
                        // No ACK received — mark as failed
                        setTxtSendState('no_ack');
                        logMessage('No ACK from device after ' + (Date.now() - app.txtTxStartTime) + 'ms');
                    }, 5000);
                }, 200);
            } else {
                addMessageToHistory('sent', data, formatTimestamp(), 'failed');
                setTxtSendState('ble_error');
                logMessage('BLE write failed');
            }
        });
        
        inputField.value = '';
        updateCharCounter(0);
    } else {
        logMessage('Please enter some text to send.');
    }
}

// TXT send state machine (mirrors PTT's state machine pattern)
function setTxtSendState(state) {
    var btn = document.getElementById('sendBtn');
    if (!btn) return;
    
    var statusLine = document.getElementById('txtStatusLine');
    if (!statusLine) return;
    
    var inputField = document.getElementById('inputField');
    
    // Clear previous state classes and timeouts
    btn.className = 'send-btn';
    clearTimeout(app.txtAckTimer);
    
    switch (state) {
        case 'sending':
            btn.classList.add('sending');
            btn.disabled = true;
            statusLine.innerHTML = '<span class="txt-status txt-sending">&#9203;</span> Sending...';
            inputField.value = '';
            break;
            
        case 'transmitting':
            app.txtTxStartTime = Date.now();
            btn.classList.add('transmitting');
            btn.disabled = true;
            statusLine.innerHTML = '<span class="txt-status txt-transmitting">&#9203;</span> Transmitting...';
            break;
            
        case 'sent':
            btn.classList.add('sent');
            btn.disabled = false;
            statusLine.innerHTML = '<span class="txt-status txt-sent">&#10003; Sent</span>';
            // Revert to normal after 2 seconds
            setTimeout(function() {
                updateCharCounter(document.getElementById('inputField').value.length);
            }, 2000);
            break;
            
        case 'no_ack':
            btn.disabled = false;
            statusLine.innerHTML = '<span class="txt-status txt-failed">&#10007; No response</span>';
            // Revert to normal after 3 seconds
            setTimeout(function() {
                updateCharCounter(document.getElementById('inputField').value.length);
            }, 3000);
            break;
            
        case 'ble_error':
            btn.disabled = false;
            statusLine.innerHTML = '<span class="txt-status txt-failed">&#10007; BLE error</span>';
            setTimeout(function() {
                updateCharCounter(document.getElementById('inputField').value.length);
            }, 3000);
            break;
            
        case 'channel_offline':
            btn.disabled = false;
            statusLine.innerHTML = '<span class="txt-status txt-failed">&#10007; No one on channel</span>';
            setTimeout(function() {
                updateCharCounter(document.getElementById('inputField').value.length);
            }, 3000);
            break;
            
        default:
            btn.disabled = false;
            statusLine.innerHTML = '';
    }
}

// TXT mode status polling (reuses PTT's GETSTATUS pattern)
var txtStatusPollingEnabled = false;
function startTxtStatusPolling(deviceName) {
    if (txtStatusPollingEnabled) return;
    txtStatusPollingEnabled = true;
    
    // Use the passed deviceName, or default to first connected device
    app.txtPollTimer = setInterval(function() {
        var target = deviceName || app.targetDeviceName;
        if (!target || !app.connectedDevices[target]) return;
        ble.write(
            app.connectedDevices[target].peripheralId,
            app.serviceUUID,
            app.characteristicUUID,
            app.stringToBytes("GETSTATUS"),
            function() {},
            function(err) { logMessage("Status poll error: " + err); }
        );
    }, 3000);
}

function stopTxtStatusPolling() {
    txtStatusPollingEnabled = false;
    if (app.txtPollTimer) { clearInterval(app.txtPollTimer); app.txtPollTimer = null; }
    // Clear status indicators
    var badge = document.getElementById('txtChannelBadge');
    if (badge) {
        badge.textContent = '\u2014';
        badge.className = 'status-badge';
    }
}

// Format timestamp for message history
function formatTimestamp() {
    var d = new Date();
    var h = String(d.getHours()).padStart(2, '0');
    var m = String(d.getMinutes()).padStart(2, '0');
    var s = String(d.getSeconds()).padStart(2, '0');
    return h + ':' + m + ':' + s;
}

// Update character counter in the input area
function updateCharCounter(len) {
    var counter = document.getElementById('charCounter');
    if (!counter) return;
    var remaining = app.TXT_MAX_CHARS - len;
    counter.textContent = remaining + ' left';
    
    // Color coding: green > 50%, yellow > 25%, red <= 25%
    counter.className = 'char-counter';
    if (remaining <= 0) {
        counter.classList.add('over-limit');
    } else if (remaining <= Math.floor(app.TXT_MAX_CHARS * 0.25)) {
        counter.classList.add('near-limit');
    } else {
        counter.classList.add('ok');
    }
}

// Log panel toggle helpers
function openLogPanel() { document.getElementById('logPanel').classList.add('open'); }
function closeLogPanel() { document.getElementById('logPanel').classList.remove('open'); }

var app = {
    // Multi-device: map of device name -> { peripheralId, status, notificationBuffer, txtLoraAlive, txtBleAlive }
    connectedDevices: {},
    
    targetDeviceName: null,  // Which device to send TXT/SETMODE to (defaults to first if null)
    reconnectDelay: 3000,
    
    serviceUUID: "1235",
    characteristicUUID: "ABCE",
    
    // TXT mode state (mirrors PTT's pttLoraAlive pattern)
    txtLoraAlive: false,
    txtBleAlive: false,
    txtPollTimer: null,
    txtSendState: 'idle',
    txtTxStartTime: 0,
    txtAckTimer: null,
    TXT_MAX_CHARS: 85,

    // PTT state
    currentMode: null,
    pttLoraAlive: false,
    pttBleAlive: false,
    pttPollTimer: null,
    pttIsCapturing: false,
    pttIsPressing: false,
    pttHoldDuration: 0,

    // Opus encode/decode
    opusEncoderInitialized: false,
    mediaRecorder: null,
    audioContext: null,
    analyserNode: null,
    micStream: null,
    pttStartMs: 0,
    
    // Helpers
    getShortDeviceId: function(deviceName) {
        if (!deviceName) return '';
        var m = deviceName.match(/LilygoT-Echo-([A-F0-9]{8})$/);
        return m ? m[1] : deviceName;
    },
    
    updateMultiDeviceStatus: function() {
        var count = Object.keys(this.connectedDevices).length;
        var ids = [];
        for (var name in this.connectedDevices) {
            if (this.connectedDevices[name]) {
                ids.push(this.getShortDeviceId(name));
            }
        }
        if (count > 0) {
            updateDeviceStatus(count + ' device(s): ' + ids.join(', '));
        } else {
            updateDeviceStatus('No devices connected');
        }
        
        // Update status dot: green if any connected, scanning if none
        var dot = document.getElementById('statusDot');
        if (count === 0) {
            setStatusIndicator('scanning');
        } else {
            setStatusIndicator('connected');
        }
        
        // Update device selector UI
        this.renderDeviceSelector();
    },
    
    renderDeviceSelector: function() {
        var section = document.getElementById('deviceSection');
        var strip = document.getElementById('deviceStrip');
        if (!section || !strip) return;
        
        var keys = Object.keys(this.connectedDevices);
        if (keys.length === 0) {
            section.style.display = 'none';
            return;
        }
        
        section.style.display = 'block';
        var html = '';
        for (var i = 0; i < keys.length; i++) {
            var name = keys[i];
            var shortId = this.getShortDeviceId(name);
            var isActive = (name === this.targetDeviceName) ? ' active' : '';
            html += '<button class="devicePill' + isActive + '" data-device="' + name + '" onclick="app.selectDevice(\'' + name.replace(/'/g, "\\'") + '\')">' +
                    '<span class="device-dot"></span><span>' + shortId + '</span></button>';
        }
        strip.innerHTML = html;
    },
    
    selectDevice: function(deviceName) {
        this.targetDeviceName = deviceName;
        logMessage("Target set to " + deviceName);
        this.renderDeviceSelector();
        
        var badge = document.getElementById('txtChannelBadge');
        if (badge) {
            badge.textContent = 'Target: ' + this.getShortDeviceId(deviceName);
            badge.className = 'status-badge connected';
        }
    },
    
    renderDeviceSelector: function() {
        var section = document.getElementById('deviceSection');
        var strip = document.getElementById('deviceStrip');
        if (!section || !strip) return;
        
        var keys = Object.keys(this.connectedDevices);
        if (keys.length === 0) {
            section.style.display = 'none';
            return;
        }
        
        section.style.display = 'block';
        var html = '';
        for (var i = 0; i < keys.length; i++) {
            var name = keys[i];
            var shortId = this.getShortDeviceId(name);
            var isActive = (name === this.targetDeviceName) ? ' active' : '';
            html += '<button class="devicePill' + isActive + '" data-device="' + name + '" onclick="app.selectDevice(\'' + name.replace(/'/g, "\\'") + '\')">' +
                    '<span class="device-dot"></span><span>' + shortId + '</span></button>';
        }
        strip.innerHTML = html;
    },

    initialize: function() {
        this.bindEvents();
        this.initDisplayName();
        
        // Alias save button
        var aliasSetBtn = document.getElementById('aliasSetBtn');
        if (aliasSetBtn) {
            aliasSetBtn.addEventListener('click', function() {
                app.saveDisplayName();
            });
        }
        
        // Buddy list refresh button
        var buddyRefreshBtn = document.getElementById('buddyRefreshBtn');
        if (buddyRefreshBtn) {
            buddyRefreshBtn.addEventListener('click', function() {
                app.syncBuddyList();
            });
        }
        
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
        
        // PTT button - hold to talk with mic capture + Opus encode
        var pttBtn = document.getElementById('pttButton');
        if (pttBtn) {
            pttBtn.addEventListener('touchstart', function(e) {
                e.preventDefault();
                if (!app.isAnyDeviceConnected() || !app.pttLoraAlive) return;
                app.p ttIsPressing = true;
                startPttCapture();
            });
            pttBtn.addEventListener('touchend', function(e) {
                e.preventDefault();
                if (app.p ttIsPressing) {
                    stopPttCapture();
                    app.p ttIsPressing = false;
                }
            });
        }
    },
    
    isAnyDeviceConnected: function() {
        var count = 0;
        for (var k in this.connectedDevices) {
            if (this.connectedDevices[k]) count++;
        }
        return count > 0;
    },
    
    getActiveDeviceId: function() {
        return this.targetDeviceName || Object.keys(this.connectedDevices)[0];
    },

    bindEvents: function() {
        document.addEventListener('deviceready', this.onDeviceReady.bind(this), false);
    },
    onDeviceReady: function() {
        this.scanForDevice();
    },
    scanForDevice: function() {
		var connectedCount = 0;
		for (var k in this.connectedDevices) {
			if (this.connectedDevices[k]) connectedCount++;
		}
		
        if (connectedCount > 0) {
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
                // Check if already connected to this device
                if (app.connectedDevices[device.name]) {
                    return;
                }
                logMessage("Device found: " + device.name);
                updateDeviceStatus("Device found, connecting...");
                setStatusIndicator('scanning');
                app.connectToDevice(device.id, device.name);
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
    connectToDevice: function(deviceId, deviceName) {
		if (app.connectedDevices[deviceName]) {
			logMessage("Already connected to " + deviceName + ". Ignoring.");
			return;
		}
		
		app.connectedDevices[deviceName] = {
            peripheralId: null,
            status: 'connecting',
            notificationBuffer: '',
            txtLoraAlive: false,
            txtBleAlive: false,
            pttLoraAlive: false,
            pttBleAlive: false,
            pttIsCapturing: false,
            pttIsPressing: false
        };

        logMessage("Attempting to connect to " + deviceName + "...");
        ble.connect(deviceId, function(peripheral) {
            if (peripheral && peripheral.id) {
                logMessage("Connected to " + peripheral.name + " (" + deviceName + ")");
                updateDeviceStatus("Connected: " + deviceName);
                setStatusIndicator('connected');

                app.connectedDevices[deviceName].peripheralId = peripheral.id;
                app.connectedDevices[deviceName].status = 'connected';

                // Auto-select first device as target if none chosen yet
                if (!app.targetDeviceName) {
                    app.targetDeviceName = deviceName;
                    logMessage("Selected " + deviceName + " as active device.");
                }

                app.updateMultiDeviceStatus();
                app.readWriteBLE(deviceName, peripheral.id);
                app.startNotification(deviceName, peripheral.id);
            } else {
                logMessage("Connected, but peripheral.id is not available for " + deviceName);
				delete app.connectedDevices[deviceName];
                app.updateMultiDeviceStatus();
            }
        }, function(error) {
            logMessage("Error connecting to " + deviceName + ": " + error);
            updateDeviceStatus("Error connecting to " + deviceName);
            setStatusIndicator('disconnected');
			delete app.connectedDevices[deviceName];
            app.updateMultiDeviceStatus();
        });
    },
    readWriteBLE: function(deviceName, deviceId) {
        ble.read(deviceId, app.serviceUUID, app.characteristicUUID, function(data) {
            var byteArray = new Uint8Array(data);
            var receivedValue = app.bytesToString(byteArray);
        }, function(error) {
            logMessage("Error reading from " + deviceName + ": " + error);
        });

        ble.isConnected(deviceId, function(connected) {
            if (!connected) {
                logMessage(deviceName + " disconnected.");
                updateDeviceStatus("Disconnected: " + deviceName);
                setStatusIndicator('connected'); // Keep connected status if others remain
                delete app.connectedDevices[deviceName];
                
                // If this was the target device, reset target to first available
                if (app.targetDeviceName === deviceName) {
                    var keys = Object.keys(app.connectedDevices);
                    app.targetDeviceName = keys.length > 0 ? keys[0] : null;
                    updateDeviceStatus("Target changed to: " + app.getShortDeviceId(app.targetDeviceName));
                }
                
                // If no devices left, restart scan
                var remaining = Object.keys(app.connectedDevices).length;
                if (remaining === 0) {
                    setStatusIndicator('scanning');
                    setTimeout(function() {
                        logMessage("Re-attempting to scan...");
                        app.scanForDevice();
                    }, app.reconnectDelay);
                } else {
                    app.updateMultiDeviceStatus();
                }
            }
        });
    },
    
    sendDataToDevice: function(deviceName, data, onComplete) {
        if (!deviceName || !this.connectedDevices[deviceName]) {
            logMessage("No device connected to send data: " + deviceName);
            if (onComplete) onComplete(false);
            return;
        }
        var peripheralId = this.connectedDevices[deviceName].peripheralId;
        if (!peripheralId) {
            logMessage("Peripheral ID not ready for " + deviceName);
            if (onComplete) onComplete(false);
            return;
        }
        
        var bytes = app.stringToBytes(data);
        ble.write(peripheralId, app.serviceUUID, app.characteristicUUID, bytes, function() {
            logMessage("Data written to " + deviceName + ": " + data);
            if (onComplete) onComplete(true);
        }, function(error) {
            logMessage("Error writing to " + deviceName + ": " + error);
            if (onComplete) onComplete(false);
        });
    },
    
    sendDataToDeviceToAll: function(data) {
        for (var name in this.connectedDevices) {
            if (this.connectedDevices[name]) {
                this.sendDataToDevice(name, data);
            }
        }
    },
    
    disconnectDevice: function(deviceName) {
        if (!deviceName || !this.connectedDevices[deviceName]) return;
        var peripheralId = this.connectedDevices[deviceName].peripheralId;
        ble.disconnect(peripheralId, function() {
            logMessage("Disconnected from " + deviceName);
            delete app.connectedDevices[deviceName];
            
            if (app.targetDeviceName === deviceName) {
                var keys = Object.keys(app.connectedDevices);
                app.targetDeviceName = keys.length > 0 ? keys[0] : null;
            }
            
            app.updateMultiDeviceStatus();
        }, function(err) {
            logMessage("Disconnect error for " + deviceName + ": " + err);
        });
    },

	startNotification: function(deviceName, deviceId) {
		logMessage("Starting notifications from " + deviceName + "...");

		ble.startNotification(deviceId, app.serviceUUID, app.characteristicUUID, function(data) {
			var byteArray = new Uint8Array(data);
			
			// Check for binary Opus frame prefix (raw BLE notify, no text wrapping)
			if (byteArray.length >= 4 && byteArray[0] === 0xFE && byteArray[1] === 0x01) {
				var payloadLen = (byteArray[3] << 8) | byteArray[2];
				if (payloadLen > 0 && byteArray.length >= 4 + payloadLen) {
					var opusBytes = new Uint8Array(byteArray.buffer, byteArray.byteOffset + 4, payloadLen);
					
					// Play via native Opus decoder on Android
					cordova.exec(function() {}, function(err) {}, 'OpusEncoder', 'playPCM', [opusBytes.buffer]);
				}
				return;
			}
			
			var receivedNotification = app.bytesToString(byteArray);

            // Route notification to the correct device's buffer
            if (!app.connectedDevices[deviceName]) {
                app.connectedDevices[deviceName] = {};
            }
            if (!app.connectedDevices[deviceName].notificationBuffer) {
                app.connectedDevices[deviceName].notificationBuffer = '';
            }
            
			app.connectedDevices[deviceName].notificationBuffer += receivedNotification;

			var endMarkerIndex = app.connectedDevices[deviceName].notificationBuffer.indexOf("~~");

			while (endMarkerIndex !== -1) {
				var completeMessage = app.connectedDevices[deviceName].notificationBuffer.slice(0, endMarkerIndex);
				app.processCompleteMessage(deviceName, completeMessage);
				app.connectedDevices[deviceName].notificationBuffer = app.connectedDevices[deviceName].notificationBuffer.slice(endMarkerIndex + 2);
				endMarkerIndex = app.connectedDevices[deviceName].notificationBuffer.indexOf("~~");
			}
		}, function(error) {
			logMessage("Error receiving notification from " + deviceName + ": " + error);
		});
	},
    
    processCompleteMessage: function(deviceName, message) {
        var shortId = this.getShortDeviceId(deviceName) || deviceName;
        
		var lineRegex = /LINE:(\w+)/;
		var textRegex = /TEXT:(.*)/;
		var statusRegex = /^OK\{BLE:(\d+)\}\{LORA:(\d+)\}/;
		var opusRegex = /^LINE:\d+\|DATA:(\xFE\x01)/;

		var lineMatch = message.match(lineRegex);
		var textMatch = message.match(textRegex);
		var statusMatch = message.match(statusRegex);
		var opusMatch = message.match(opusRegex);

        // Update this device's BLE/LoRa liveness state
        if (this.connectedDevices[deviceName]) {
            if (statusMatch) {
                this.connectedDevices[deviceName].txtBleAlive = parseInt(statusMatch[1]) === 1;
                this.connectedDevices[deviceName].txtLoraAlive = parseInt(statusMatch[2]) === 1;
            }
        }
        
        // Handle Opus audio frame received from firmware via BLE notify
		if (opusMatch) {
			var dataMarker = message.indexOf('|DATA:');
			if (dataMarker !== -1) {
				hexData = message.slice(dataMarker + 6);
				
				var opusBytes = [];
				for (var i = 0; i < hexData.length; i += 2) {
					if (i + 1 < hexData.length) {
						opusBytes.push(parseInt(hexData.substr(i, 2), 16));
					}
				}
				
				if (opusBytes.length >= 4 && opusBytes[0] === 0xFE && opusBytes[1] === 0x01) {
					var payloadLen = (opusBytes[3] << 8) | opusBytes[2];
					var actualOpusData = new Uint8Array(opusBytes.slice(4, 4 + payloadLen));
					
					if (actualOpusData.length > 0) {
						var arrayBuffer = actualOpusData.buffer;
						cordova.exec(function() {}, function(err) {}, 'OpusEncoder', 'playPCM', [arrayBuffer]);
						logMessage('Playing audio from ' + shortId + ': ' + payloadLen + ' bytes');
					}
				}
			}
			return;
		}

		// Handle PTT/TXT status response (GETSTATUS)
		if (statusMatch) {
			var bleAlive = parseInt(statusMatch[1]) === 1;
			var loraAlive = parseInt(statusMatch[2]) === 1;
            
            // Update this device's state
            if (this.connectedDevices[deviceName]) {
                this.connectedDevices[deviceName].txtBleAlive = bleAlive;
                this.connectedDevices[deviceName].txtLoraAlive = loraAlive;
                this.connectedDevices[deviceName].pttBleAlive = bleAlive;
                this.connectedDevices[deviceName].pttLoraAlive = loraAlive;
            }
            
            // Update global state from the active/target device
            var activeDeviceName = app.getActiveDeviceId() || deviceName;
            if (activeDeviceName === deviceName) {
				app.txtLoraAlive = loraAlive;
				app.txtBleAlive = bleAlive;
                
                var badge = document.getElementById('txtChannelBadge');
                if (badge) {
                    if (loraAlive) {
                        badge.textContent = '\u2713 On channel';
                        badge.className = 'status-badge connected';
                    } else {
                        badge.textContent = '\u2717 No one on channel';
                        badge.className = 'status-badge disconnected';
                    }
                }
                
                // Update PTT button state if in PTT mode
                if (app.currentMode === 'PTT') {
                    app.updatePttButtonState(bleAlive, loraAlive);
                }
            } else {
                // For non-active device, show status as info log
                logMessage(shortId + (statusMatch[2] === '1' ? ' is on channel' : ' offline'));
            }
            
			return;
		}

		// Handle TXT send confirmation (device echoes back "Sent: {text}")
		if (lineMatch && lineMatch[1] === 'TX') {
			var sentText = '';
			var dataMarker2 = message.indexOf('|DATA:');
			if (dataMarker2 !== -1) {
				sentText = message.slice(dataMarker2 + 6);
			} else {
				if (textMatch) {
					sentText = textMatch[1];
				}
			}
			
			// Only process ACK if from the target device
			var activeDeviceName = app.getActiveDeviceId();
            if (activeDeviceName && activeDeviceName !== deviceName) return;
            
			if (app.txtAckTimer) {
				clearTimeout(app.txtAckTimer);
				app.txtAckTimer = null;
			}
			
			if (app.txtSendState === 'transmitting') {
				app.setTxtSendState('sent');
				for (var i = messageHistory.length - 1; i >= 0; i--) {
					if (messageHistory[i].type === 'sent' && messageHistory[i].status === 'pending') {
						messageHistory[i].status = 'sent';
                        messageHistory[i].deviceName = app.getShortDeviceId(deviceName);
						updateMessageHistoryUI();
						break;
					}
				}
			}
			return;
		}

        // Handle buddy list response from device
		if (lineMatch && textMatch) {
			var data = textMatch[1] || '';
			if (data.match(/^OK\{BUDDY:/)) {
				// Parse buddy CSV or count
				var match2 = data.match(/OK\{BUDDY:(.*)\}/);
				if (match2) {
					var csvData = match2[1];
					app.buddyListReceived = true;
					
					if (csvData.length > 0 && csvData.indexOf(',') >= 0) {
						// Parse CSV: "CNname1|DIid1,CNname2|DIid2,..."
						var entries = csvData.split(',');
						app.buddyList = [];
						for (var i = 0; i < entries.length; i++) {
							var entry = entries[i].trim();
							if (!entry) continue;
							var cnMatch = entry.match(/^CN(.*)\|DI([A-F0-9]+)$/);
							if (cnMatch) {
								app.buddyList.push({ call_sign: cnMatch[1], device_id: cnMatch[2] });
							}
						}
					} else if (data.match(/OK\{BUDDY:(\d+)\}/)) {
						// It was a count format "OK{BUDDY:5}" — import from SENDTXT action on device
						app.buddyList = [{ call_sign: '[DEVICE]', device_id: '' }];
					}
					
					logMessage('Contacts synced: ' + app.buddyList.length + ' entries');
					app.updateBuddyListUI();
				}
				return;
			}
			
			// Handle SETNAME ACK
			if (data.match(/^OK\{NAME:/)) {
				var nameMatch = data.match(/OK\{NAME:(.*?)\}/);
				if (nameMatch) {
					logMessage('Alias set: ' + nameMatch[1]);
					showToast('Alias saved');
				}
				return;
			}
		}

		// Handle Waypoint received from device
		if (lineMatch && lineMatch[1] === 'WP') {
			var dataMarker = message.indexOf('|DATA:');
			if (dataMarker !== -1) {
				var wpRaw = message.slice(dataMarker + 6);
				
				// Parse: "label lat,lon alt:Xm from sender" or "Waypoint lat,lon alt:Xm from sender"
				var wpData = app.parseWaypointData(wpRaw);
				if (wpData) {
					app.waypoints.push({
						id: shortId + '_' + Date.now(),
						lat: wpData.lat,
						lon: wpData.lon,
						alt: wpData.alt,
						label: wpData.label,
						sender: wpData.sender || shortId,
						receivedAt: formatTimestamp()
					});
					app.renderWaypointMap();
					showToast('WP from ' + shortId);
				}
			}
			return;
		}

		if (lineMatch && textMatch) {
			var lineNumber = parseInt(lineMatch[1], 10);
			var text = textMatch ? textMatch[1] : '';
			
			if(lineNumber<11) {
				document.getElementById('line' + lineNumber).textContent = text;
				document.getElementById('screen').classList.remove('empty-hint');
			}
			
			// Add received TXT messages to history (from any device, but only line 4 = content lines)
			if (lineNumber === 4 && app.currentMode !== 'PTT') {
				var cleanText = text.replace(/^Sent:\s*/, '');
                var displayName = app.getShortDeviceId(deviceName);
                
                // If the received message is from a device OTHER than our target, it's a LoRa receive
                var activeDeviceName = app.getActiveDeviceId();
                if (activeDeviceName && activeDeviceName !== deviceName && cleanText.length > 0) {
                    addMessageToHistory('received', '[LoRa via ' + displayName + '] ' + cleanText, formatTimestamp(), 'read');
                    showToast('Received from ' + displayName);
                } else if (!activeDeviceName || activeDeviceName === deviceName) {
                    // Echo ACK from our target device — just log it
                    logMessage("Echo from " + displayName + ": " + cleanText);
                }
			}
		} else {
            var displayName = app.getShortDeviceId(deviceName);
			updateDeviceInfo(displayName + ': ' + message);
			logMessage("Received from " + shortId + ": " + message);
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
	},

    // === TXT Mode Methods (mirrors PTT pattern) ===
    startTxtStatusPolling: function(deviceName) {
        if (app.txtPollTimer) clearInterval(app.txtPollTimer);
        app.txtPollTimer = setInterval(function() {
            var target = deviceName || app.targetDeviceName;
            if (!target || !app.connectedDevices[target]) return;
            ble.write(
                app.connectedDevices[target].peripheralId,
                app.serviceUUID,
                app.characteristicUUID,
                app.stringToBytes("GETSTATUS"),
                function() {},
                function(err) { logMessage("Status poll error: " + err); }
            );
        }, 3000);
    },
    stopTxtStatusPolling: function() {
        if (app.txtPollTimer) { clearInterval(app.txtPollTimer); app.txtPollTimer = null; }
    },

    // === Buddy List / Call Sign ===
    displayName: '',
    buddyList: [],  // [{call_sign, device_id}] from peer beacons
    buddyListReceived: false,

    initDisplayName: function() {
        var saved = localStorage.getItem('ptt_buddy_name');
        if (saved) {
            app.displayName = saved;
            document.getElementById('myAliasInput').value = saved;
        } else {
            // Default to last 4 of MAC
            var mac = device ? device.name.replace(/^LilygoT-Echo-/, '').substr(-4) : '';
            if (mac) {
                app.displayName = mac.toUpperCase();
                document.getElementById('myAliasInput').value = mac;
            }
        }
    },

    saveDisplayName: function() {
        var name = document.getElementById('myAliasInput').value.trim();
        if (!name || name.length > 16) {
            showToast('Alias must be 1-16 characters');
            return;
        }
        app.displayName = name;
        localStorage.setItem('ptt_buddy_name', name);
        
        // Send to device over BLE
        var target = app.getActiveDeviceId();
        if (target) {
            ble.write(
                app.connectedDevices[target].peripheralId,
                app.serviceUUID,
                app.characteristicUUID,
                app.stringToBytes('SETNAME:' + name),
                function() {},
                function(err) { logMessage('Alias set error: ' + err); }
            );
        }
        
        showToast('Alias saved: ' + name);
    },

    syncBuddyList: function() {
        var target = app.getActiveDeviceId();
        if (!target || !app.connectedDevices[target]) return;
        
        // Request buddy list from device
        ble.write(
            app.connectedDevices[target].peripheralId,
            app.serviceUUID,
            app.characteristicUUID,
            app.stringToBytes('GETBUDDY'),
            function() {},
            function(err) { logMessage('Buddy sync error: ' + err); }
        );
        
        showToast('Syncing contacts...');
    },

    updateBuddyListUI: function() {
        var section = document.getElementById('buddySection');
        var container = document.getElementById('buddyContainer');
        if (!section || !container) return;
        
        // Merge peer beacons with buddy list — use display names where available
        var contacts = [];
        
        // Add known buddy list entries from device
        for (var i = 0; i < app.buddyList.length; i++) {
            var b = app.buddyList[i];
            if (!b.call_sign) continue;
            // Check if already in contacts by device_id
            var found = false;
            for (var j = 0; j < contacts.length; j++) {
                if (contacts[j].device_id === b.device_id) { found = true; break; }
            }
            if (!found) contacts.push(b);
        }
        
        // Add peers from connected devices as potential contacts
        for (var name in app.connectedDevices) {
            var dev = app.connectedDevices[name];
            if (!dev) continue;
            var shortId = app.getShortDeviceId(name);
            // Check if already in contacts
            var found = false;
            for (var i2 = 0; i2 < contacts.length; i2++) {
                if (contacts[i2].device_id === shortId) { found = true; break; }
            }
            if (!found) {
                contacts.push({ call_sign: shortId, device_id: shortId });
            }
        }
        
        // Add local display name entry
        if (app.displayName) {
            var selfFound = false;
            for (var i3 = 0; i3 < contacts.length; i3++) {
                if (contacts[i3].call_sign === app.displayName) { selfFound = true; break; }
            }
            if (!selfFound) contacts.push({ call_sign: '[ME]', device_id: '' });
        }
        
        section.style.display = 'block';
        
        var html = '';
        for (var k = 0; k < contacts.length; k++) {
            var c = contacts[k];
            var isSelf = (c.call_sign === app.displayName) || (c.device_id === '');
            var cls = isSelf ? 'buddy-contact self' : 'buddy-contact';
            html += '<div class="' + cls + '">';
            html += '<span class="buddy-name">' + escapeHtml(c.call_sign) + '</span>';
            if (c.device_id && !isSelf) {
                html += '<span class="buddy-id">[' + c.device_id + ']</span>';
            }
            html += '</div>';
        }
        
        if (contacts.length === 0) {
            html = '<div class="buddy-empty">No contacts yet. Send a beacon to discover peers.</div>';
        }
        
        container.innerHTML = html;
    },

    // === PTT Methods ===
    startPttStatusPolling: function() {
        if (app.pttPollTimer) clearInterval(app.pttPollTimer);
        app.pttPollTimer = setInterval(function() {
            var target = app.getActiveDeviceId();
            if (!target || !app.connectedDevices[target]) return;
            ble.write(
                app.connectedDevices[target].peripheralId,
                app.serviceUUID,
                app.characteristicUUID,
                app.stringToBytes("GETSTATUS"),
                function() {},
                function(err) { logMessage("Status poll error: " + err); }
            );
        }, 3000);
    },
    stopPttStatusPolling: function() {
        if (app.pttPollTimer) { clearInterval(app.pttPollTimer); app.pttPollTimer = null; }
    },
    updatePttButtonState: function(bleAlive, loraAlive) {
        app.p ttBleAlive = bleAlive;
        app.p ttLoraAlive = loraAlive;
        
        var pttBtn = document.getElementById('pttButton');
        var pttLabel = document.getElementById('pttLabel');
        var bleBadge = document.getElementById('bleBadge');
        var loraBadge = document.getElementById('loraBadge');
        
        if (bleAlive) {
            bleBadge.textContent = 'Connected';
            bleBadge.className = 'status-badge connected';
        } else {
            bleBadge.textContent = 'Disconnected';
            bleBadge.className = 'status-badge disconnected';
        }
        
        if (loraAlive) {
            loraBadge.textContent = 'Ready to talk';
            loraBadge.className = 'status-badge connected';
        } else {
            loraBadge.textContent = 'No one on channel';
            loraBadge.className = 'status-badge disconnected';
        }
        
        if (!bleAlive) {
            pttBtn.disabled = true;
            pttBtn.className = '';
            pttLabel.textContent = 'Tap to connect';
        } else if (!loraAlive) {
            pttBtn.disabled = true;
            pttBtn.className = '';
            pttLabel.textContent = 'No one on channel';
        } else if (!app.p ttIsCapturing) {
            pttBtn.disabled = false;
            pttBtn.className = 'active';
            pttLabel.textContent = 'HOLD TO TALK';
        }
    },
    updatePttTransmitState: function(isActive) {
        var pttBtn = document.getElementById('pttButton');
        var pttLabel = document.getElementById('pttLabel');
        
        if (isActive) {
            app.p ttIsCapturing = true;
            pttBtn.className = 'transmitting';
            pttLabel.textContent = 'TRANSMITTING...';
        } else {
            app.p ttIsCapturing = false;
            pttLabel.textContent = 'SENT';
            setTimeout(function() {
                if (app.p ttLoraAlive && app.p ttBleAlive) {
                    pttBtn.className = 'active';
                    pttLabel.textContent = 'HOLD TO TALK';
                }
            }, 800);
        }
    },
    
    // Start mic capture + init Opus encoder
    startPttCapture: function() {
        if (app.p ttIsCapturing) return;
        
        app.updatePttTransmitState(true);
        app.pttStartMs = Date.now();
        
        var success = function(stream) {
            app.micStream = stream;
            
            if (!app.opusEncoderInitialized) {
                OpusEncoder.init(function() {
                    app.opusEncoderInitialized = true;
                    startOpusEncoding();
                });
            } else {
                startOpusEncoding();
            }
        };
        
        var fail = function(err) {
            logMessage('Mic error: ' + err);
            app.updatePttTransmitState(false);
        };
        
        navigator.mediaDevices.getUserMedia({ audio: true, video: false })
            .then(success).catch(fail);
        
        function startOpusEncoding() {
            // Create AudioContext for mic processing
            if (!app.audioContext) {
                app.audioContext = new (window.AudioContext || window.webkitAudioContext)();
            }
            
            var source = app.audioContext.createMediaStreamSource(app.micStream);
            app.analyserNode = app.audioContext.createScriptProcessor(4096, 1, 1);
            source.connect(app.analyserNode);
            
            var source = app.audioContext.createMediaStreamSource(app.micStream);
            app.analyserNode = app.audioContext.createScriptProcessor(4096, 1, 1);
            source.connect(app.analyserNode);
            
            // Capture PCM via ScriptProcessorNode audioprocess event (getFloat32Data doesn't exist)
            var opusInterval;
            app.analyserNode.onaudioprocess = function(e) {
                if (!app.p ttIsCapturing) return;
                
                var inputData = e.inputBuffer.getChannelData(0);
                var bufferSize = 160; // 20ms @ 8kHz mono int16
                var pcm8k = new Int16Array(bufferSize);
                
                // Resample 48kHz→8kHz (simple decimation by 6)
                for (var i = 0; i < pcm8k.length && i * 6 < inputData.length; i++) {
                    pcm8k[i] = Math.max(-32768, Math.min(32767, inputData[i * 6] * 32767));
                }
                
                // Convert to byte array (little-endian) for native plugin
                var pcmBytes = new Uint8Array(pcm8k.length * 2);
                for (var i = 0; i < pcm8k.length; i++) {
                    pcmBytes[i*2] = pcm8k[i] & 0xFF;
                    pcmBytes[i*2+1] = (pcm8k[i] >> 8) & 0xFF;
                }
                
                if (app.opusEncoderInitialized && OpusEncoder.encodeFrame) {
                    var opusResult = new Promise(function(resolve) {
                        cordova.exec(function(result) { resolve(result[0]); }, 
                                      function() { resolve(null); }, 
                                      'OpusEncoder', 'encodeFrame', [pcmBytes.buffer]);
                    });
                    
                    opusResult.then(function(opusBytes) {
                        if (opusBytes && app.p ttIsCapturing) {
                            app.sendOpusFrame(opusBytes);
                        }
                    });
                }
            };
            
            app.pttCaptureInterval = opusInterval;
        }
    },
    
    stopPttCapture: function() {
        if (app.p ttIsCapturing) {
            // Stop capture event handler
            if (app.analyserNode) {
                app.analyserNode.onaudioprocess = null;
                app.analyserNode.disconnect();
            }
            
            // Release mic stream
            if (app.micStream) {
                app.micStream.getTracks().forEach(function(track) { track.stop(); });
            }
            
            OpusEncoder.releaseAll();
            app.opusEncoderInitialized = false;
            app.micStream = null;
            app.audioContext = null;
            app.analyserNode = null;
        }
        
        app.updatePttTransmitState(false);
    },
    
    sendOpusFrame: function(opusBytes) {
        var targetDeviceName = app.getActiveDeviceId();
        if (!targetDeviceName || !app.connectedDevices[targetDeviceName] || !opusBytes) return;
        
        var peripheralId = app.connectedDevices[targetDeviceName].peripheralId;
        var len = opusBytes.byteLength || opusBytes.length;
        var header = new Uint8Array(4);
        header[0] = 0xFE;
        header[1] = 0x01;
        header[2] = len & 0xFF;
        header[3] = (len >> 8) & 0xFF;
        
        var packet = new Uint8Array(4 + len);
        packet.set(header);
        if (opusBytes.byteLength !== undefined && opusBytes.buffer) {
            packet.set(new Uint8Array(opusBytes), 4);
        } else {
            var arr = new Uint8Array(opusBytes);
            packet.set(arr, 4);
        }
        
        ble.write(
            peripheralId,
            app.serviceUUID,
            app.characteristicUUID,
            packet.buffer,
            function() {},
            function(err) { logMessage('BLE write error: ' + err); }
        );
    },

    // Play received Opus via native decoder + AudioTrack
    playReceivedOpus: function(opusBytes) {
        return new Promise(function(resolve, reject) {
            cordova.exec(function(result) {
                resolve(result[0]); // decoded PCM bytes (played by native plugin)
            }, reject, 'OpusEncoder', 'playPCM', [opusBytes]);
        });
    }
};
