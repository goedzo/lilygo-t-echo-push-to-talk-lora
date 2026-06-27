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
            html += '<div class="history-msg-status">' + statusIcon + ' ' + m.status + '</div>';
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
	app.sendDataToDevice("SETMODE:"+aMode);
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
		app.startTxtStatusPolling();
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
        app.sendDataToDevice("SENDTXT:"+data, function(success) {
            if (success) {
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
function startTxtStatusPolling() {
    if (txtStatusPollingEnabled) return;
    txtStatusPollingEnabled = true;
    
    app.txtLoraAlive = false;
    app.txtBleAlive = false;
    
    // Reset TXT send state to idle
    setTxtSendState('idle');
    document.getElementById('txtChannelBadge').textContent = 'Checking...';
    document.getElementById('txtChannelBadge').className = 'status-badge checking';
}

function stopTxtStatusPolling() {
    txtStatusPollingEnabled = false;
    if (app.txtPollTimer) { clearInterval(app.txtPollTimer); app.txtPollTimer = null; }
    // Clear status indicators
    var badge = document.getElementById('txtChannelBadge');
    if (badge) {
        badge.textContent = '—';
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
    connectedDeviceId: null,
    reconnectDelay: 3000,
    isDeviceConnected: false,
    serviceUUID: "1235",
    characteristicUUID: "ABCE",
	notificationBuffer: "",
    
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
        
        // PTT button - hold to talk with mic capture + Opus encode
        var pttBtn = document.getElementById('pttButton');
        if (pttBtn) {
            pttBtn.addEventListener('touchstart', function(e) {
                e.preventDefault();
                if (!app.isDeviceConnected || !app.pttLoraAlive) return;
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
    sendDataToDevice: function(data, onComplete) {
        if (!app.connectedDeviceId) {
            logMessage("No device connected to send data.");
            if (onComplete) onComplete(false);
            return;
        }
        var bytes = app.stringToBytes(data);
        ble.write(app.connectedDeviceId, app.serviceUUID, app.characteristicUUID, bytes, function() {
            logMessage("Data written: " + data);
            if (onComplete) onComplete(true);
        }, function(error) {
            logMessage("Error writing data: " + error);
            if (onComplete) onComplete(false);
        });
    },
	startNotification: function(deviceId) {
		logMessage("Starting notifications from device...");

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
			var lineRegex = /LINE:(\w+)/;
			var textRegex = /TEXT:(.*)/;
			var statusRegex = /^OK\{BLE:(\d+)\}\{LORA:(\d+)\}/;
			var opusRegex = /^LINE:\d+\|DATA:(\xFE\x01)/;

			var lineMatch = message.match(lineRegex);
			var textMatch = message.match(textRegex);
			var statusMatch = message.match(statusRegex);
			var opusMatch = message.match(opusRegex);

			// Handle Opus audio frame received from firmware via BLE notify
			if (opusMatch) {
				// Extract hex-encoded Opus bytes after the \xFE\x01 prefix
				var hexData = message.slice(message.indexOf('\xFE\x01') + 4); // skip prefix and "DATA:" marker
				// Actually find the part after the DATA: marker in the LINE format
				// Format is: LINE:9|DATA:\xFE\x01[hex bytes]...
				var dataMarker = message.indexOf('|DATA:');
				if (dataMarker !== -1) {
					hexData = message.slice(dataMarker + 6); // skip past "|DATA:"
					
					// Parse hex-encoded binary back to byte array
					var opusBytes = [];
					for (var i = 0; i < hexData.length; i += 2) {
						if (i + 1 < hexData.length) {
							opusBytes.push(parseInt(hexData.substr(i, 2), 16));
						}
					}
					
					if (opusBytes.length >= 4 && opusBytes[0] === 0xFE && opusBytes[1] === 0x01) {
						var payloadLen = (opusBytes[3] << 8) | opusBytes[2];
						var actualOpusData = new Uint8Array(opusBytes.slice(4, 4 + payloadLen));
						
						// Play through native Opus decoder on Android
						if (actualOpusData.length > 0) {
							var arrayBuffer = actualOpusData.buffer;
							cordova.exec(function() {}, function(err) {}, 'OpusEncoder', 'playPCM', [arrayBuffer]);
							logMessage('Playing audio: ' + payloadLen + ' bytes');
						}
					}
				}
				return;
			}

			// Handle PTT/TXT status response (GETSTATUS)
			if (statusMatch) {
				var bleAlive = parseInt(statusMatch[1]) === 1;
				var loraAlive = parseInt(statusMatch[2]) === 1;
				
				if (app.currentMode === 'PTT') {
					app.updatePttButtonState(bleAlive, loraAlive);
				}
				
				// TXT mode: update channel liveness badge
				if (app.currentMode === 'TXT' || app.currentMode === null) {
					app.txtLoraAlive = loraAlive;
					app.txtBleAlive = bleAlive;
					
					var badge = document.getElementById('txtChannelBadge');
					if (badge) {
						if (loraAlive) {
							badge.textContent = '✓ On channel';
							badge.className = 'status-badge connected';
						} else {
							badge.textContent = '✗ No one on channel';
							badge.className = 'status-badge disconnected';
						}
					}
					
					// If we just received a valid GETSTATUS and were waiting for ACK,
					// that means the device is alive. If we're in TXT transmit state,
					// check if the peer (other device) is also alive to infer delivery.
					if (app.txtSendState === 'transmitting' && loraAlive) {
						// Peer might be on channel — give a bit more time for ACK
						// If the other side received, they'll respond with their own status
					}
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
					// Try TEXT: format
					if (textMatch) {
						sentText = textMatch[1];
					}
				}
				
				// Clear the ACK timer
				if (app.txtAckTimer) {
					clearTimeout(app.txtAckTimer);
					app.txtAckTimer = null;
				}
				
				// Update send state to "sent"
				if (app.txtSendState === 'transmitting') {
					app.setTxtSendState('sent');
					// Update the message history entry for the last sent message
					for (var i = messageHistory.length - 1; i >= 0; i--) {
						if (messageHistory[i].type === 'sent' && messageHistory[i].status === 'pending') {
							messageHistory[i].status = 'sent';
							updateMessageHistoryUI();
							break;
						}
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
				
				// Also add received TXT messages to history
				if (lineNumber === 4 && app.currentMode !== 'PTT') {
					var cleanText = text.replace(/^Sent:\s*/, '');
					if (cleanText.length > 0) {
						addMessageToHistory('received', cleanText, formatTimestamp(), 'read');
					}
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
	},

    // === TXT Mode Methods (mirrors PTT pattern) ===
    startTxtStatusPolling: function() {
        if (app.txtPollTimer) clearInterval(app.txtPollTimer);
        app.txtPollTimer = setInterval(function() {
            if (!app.isDeviceConnected || !app.connectedDeviceId) return;
            ble.write(
                app.connectedDeviceId,
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

    // === PTT Methods ===
    startPttStatusPolling: function() {
        if (app.pttPollTimer) clearInterval(app.pttPollTimer);
        app.pttPollTimer = setInterval(function() {
            if (!app.isDeviceConnected || !app.connectedDeviceId) return;
            ble.write(
                app.connectedDeviceId,
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
        if (!app.connectedDeviceId || !opusBytes) return;
        
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
            // opusBytes might be ArrayBuffer from native plugin
            var arr = new Uint8Array(opusBytes);
            packet.set(arr, 4);
        }
        
        ble.write(
            app.connectedDeviceId,
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
