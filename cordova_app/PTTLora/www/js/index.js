document.addEventListener('deviceready', function() {
    app.onDeviceReady();
}, false);

// Capture unhandled JavaScript errors and console exceptions
window.onerror = function(message, source, line, col, error) {
    logMessage('[JS ERROR] ' + message + (source ? ' at ' + source : '') + ':' + line);
    return false;
};
window.addEventListener('unhandledrejection', function(e) {
    logMessage('[JS UNHANDLED REJECTION] ' + (e.reason ? String(e.reason) : 'unknown'));
});

// Add message to console
function logMessage(message) {
    console.log(message);
    const consoleDiv = document.getElementById('console');
    const newMessage = document.createElement('p');
    newMessage.textContent = message;
    consoleDiv.appendChild(newMessage);
    consoleDiv.scrollTop = consoleDiv.scrollHeight;
}

// Show toast notification
function showToast(msg) {
    var el = document.getElementById('toast');
    if (!el) return;
    el.textContent = msg;
    el.classList.add('visible');
    clearTimeout(el._timer);
    el._timer = setTimeout(function() {
        el.classList.remove('visible');
    }, 2000);
}

// Copy log to clipboard
function copyLogToClipboard() {
    var consoleDiv = document.getElementById('console');
    if (!consoleDiv) return;
    
    var lines = consoleDiv.querySelectorAll('p');
    var text = '';
    for (var i = 0; i < lines.length; i++) {
        text += lines[i].textContent + '\n';
    }
    
    if (navigator.clipboard && navigator.clipboard.writeText) {
        navigator.clipboard.writeText(text).then(function() {
            showToast('Log copied to clipboard');
            var btn = document.getElementById('logCopyBtn');
            if (btn) { btn.classList.add('copied'); setTimeout(function(){ btn.classList.remove('copied'); }, 1500); }
        }).catch(function() {
            fallbackCopy(text);
        });
    } else {
        fallbackCopy(text);
    }
}

function fallbackCopy(text) {
    var ta = document.createElement('textarea');
    ta.value = text;
    ta.style.position = 'fixed';
    ta.style.left = '-9999px';
    document.body.appendChild(ta);
    ta.select();
    try {
        document.execCommand('copy');
        showToast('Log copied to clipboard');
        var btn = document.getElementById('logCopyBtn');
        if (btn) { btn.classList.add('copied'); setTimeout(function(){ btn.classList.remove('copied'); }, 1500); }
    } catch(e) {
        showToast('Failed to copy log');
    }
    document.body.removeChild(ta);
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
var logIcons = {
    open: '<svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><polyline points="8 6 16 6 16 18 8 18 8 6"/><line x1="12" y1="2" x2="12" y2="6"/><line x1="12" y1="18" x2="12" y2="22"/></svg>',
    close: '<svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><polyline points="4 7 4 4 20 4 20 7"/><line x1="9" y1="20" x2="15" y2="20"/><line x1="12" y1="4" x2="12" y2="20"/></svg>'
};

function openLogPanel() { document.getElementById('logPanel').classList.add('open'); updateLogButton(); }
function closeLogPanel() { document.getElementById('logPanel').classList.remove('open'); updateLogButton(); }

function updateLogButton() {
    var panel = document.getElementById('logPanel');
    var btn = document.getElementById('logToggle');
    if (!panel || !btn) return;
    var isOpen = panel.classList.contains('open');
    btn.innerHTML = isOpen ? logIcons.close : logIcons.open;
    btn.title = isOpen ? 'Hide log' : 'Show log';
}

function toggleLogPanel() {
    var panel = document.getElementById('logPanel');
    if (!panel) return;
    if (panel.classList.contains('open')) {
        closeLogPanel();
    } else {
        openLogPanel();
    }
}

var app = {
    // Multi-device: map of device name -> { peripheralId, status, notificationBuffer, txtLoraAlive, txtBleAlive }
    connectedDevices: {},
    
    targetDeviceName: null,  // Which device to send TXT/SETMODE to (defaults to first if null)
    reconnectDelay: 3000,
    
    serviceUUID: "1235",
    characteristicUUID: "abce",
    
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
        var m = deviceName.match(/LilygoT-Echo-([A-F0-9]{8})$/i);
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
        // Log panel toggle
        var logToggleBtn = document.getElementById('logToggle');
        if (logToggleBtn) {
            logToggleBtn.onclick = function(e) { e.preventDefault(); e.stopPropagation(); toggleLogPanel(); return false; };
        }
        
        // Log copy button
        var logCopyBtn = document.getElementById('logCopyBtn');
        if (logCopyBtn) {
            logCopyBtn.onclick = function(e) { e.preventDefault(); e.stopPropagation(); copyLogToClipboard(); return false; };
        }
        
        // Initialize log button icon to match current panel state
        updateLogButton();
        
        // PTT button - hold to talk with mic capture + Opus encode
        var pttBtn = document.getElementById('pttButton');
        if (pttBtn) {
            pttBtn.addEventListener('touchstart', function(e) {
                e.preventDefault();
                if (!app.isAnyDeviceConnected() || !app.pttLoraAlive) return;
                app.pttIsPressing = true;
                startPttCapture();
            });
            pttBtn.addEventListener('touchend', function(e) {
                e.preventDefault();
                if (app.pttIsPressing) {
                    stopPttCapture();
                    app.pttIsPressing = false;
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
        // All other event listeners are bound here via app.initialize()
    },
    onDeviceReady: function() {
        logMessage('Cordova is ready.');
        updateDeviceStatus('Device ready, scanning for BLE devices...');
        setStatusIndicator('scanning');
        this.initialize();
        logMessage('Before scanForDevice timeout');
        var self = this;
        setTimeout(function() { 
            logMessage('Scan timeout fired, calling scanForDevice');
            self.scanForDevice(); 
        }, 500);
    },
    scanForDevice: function() {
        logMessage('scanForDevice called');
        var connectedCount = 0;
        for (var k in this.connectedDevices) {
            if (this.connectedDevices[k]) connectedCount++;
        }
        
        if (connectedCount > 0) {
            logMessage('Already have ' + connectedCount + ' connected device(s), skipping scan');
            return;
        }

        logMessage("Scanning for BLE devices...");
        var serviceUUIDs = ["1235"];
        ble.startScan(serviceUUIDs, function(device) {
            logMessage("Device discovered: " + device.name + " (id=" + device.id + ")");
            if (device.name && app.isValidDeviceName(device.name)) {
                // Check if already connected to this device
                if (app.connectedDevices[device.name]) {
                    return;
                }
                logMessage("Matching device found: " + device.name);
                updateDeviceStatus("Device found, connecting...");
                setStatusIndicator('scanning');
                // Connect immediately without stopScan — the plugin needs the scan handle alive
                app.connectToDevice(device.id, device.name);
            }
        }, function(error) {
            logMessage("BLE scan error: " + error);
            updateDeviceStatus("Scan error, retrying...");
            setStatusIndicator('disconnected');
            setTimeout(function() {
                logMessage("Re-attempting to scan...");
                app.scanForDevice();
            }, app.reconnectDelay);
        });
    },
    isValidDeviceName: function(deviceName) {
        const pattern = /^LilygoT-Echo-[A-F0-9]{8}$/i;
        return pattern.test(deviceName);
    },
    connectToDevice: function(deviceId, deviceName) {
		if (app.connectedDevices[deviceName]) {
			logMessage("Already connected to " + deviceName + ". Ignoring.");
			return;
		}
		
        app.connectedDevices[deviceName] = {
            peripheralId: deviceId,
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
        ble.connect(deviceId, function(peripheralId) {
            logMessage("Connected to " + deviceName);
            updateDeviceStatus("Connected: " + deviceName);
            setStatusIndicator('connected');

            app.connectedDevices[deviceName].status = 'connected';

            // Auto-select first device as target if none chosen yet
            if (!app.targetDeviceName) {
                app.targetDeviceName = deviceName;
                logMessage("Selected " + deviceName + " as active device.");
            }

            app.updateMultiDeviceStatus();
            // Wait for GATT service discovery to fully complete before touching the characteristic
            setTimeout(function() {
                logMessage("Waiting 1s for BLE stack stability before enabling notifications...");
                setTimeout(function() {
                    app.startNotification(deviceName, deviceId);
                }, 1000);
            }, 1500);
        }, function(error) {
            var errMsg = typeof error === 'string' ? error : (error.message || error.code != null ? 'code=' + error.code : '') + (error.resultCode != null ? ', resultCode=' + error.resultCode : '') + (error.targetDeviceName && typeof error.targetDeviceName !== 'function' ? ', target=' + error.targetDeviceName : '') || Object.keys(error).length ? JSON.stringify(error) : 'unknown BLE error';
            logMessage("Error connecting to " + deviceName + ": " + errMsg);
            updateDeviceStatus("Error connecting to " + deviceName);
            setStatusIndicator('disconnected');
			delete app.connectedDevices[deviceName];
            app.updateMultiDeviceStatus();
            // Restart scan to find and reconnect to the device
            setTimeout(function() {
                logMessage("Re-attempting to scan after connection failure...");
                app.scanForDevice();
            }, app.reconnectDelay);
        });
    },
    readWriteBLE: function(deviceName, deviceId) {
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

		var self = this;

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
			logMessage("Error receiving notification from " + deviceName + ": " + error + ", retrying in 1s...");
			setTimeout(function() {
				app.startNotification(deviceName, deviceId);
			}, 1000);
		});

		// Poll for screen data via GETSCREEN write (in case notifications don't work on this device)
		var screenPollCount = 0;
		function pollScreen() {
			if (!self.connectedDevices[deviceName]) return;
			screenPollCount++;
			ble.write(
				deviceId, app.serviceUUID, app.characteristicUUID,
				self.stringToBytes("GETSCREEN"),
				function() {
					logMessage("GETSCREEN#" + screenPollCount + " sent to " + deviceName);
					// Keep polling every 5s as a fallback in case push notifications never arrive
					if (screenPollCount < 30) setTimeout(pollScreen, 5000);
				},
				function(err) {
					logMessage("GETSCREEN write failed (" + err + ")");
				}
			);
		}
		// Start polling after a delay to give the BLE stack time to settle
		setTimeout(pollScreen, 2000);
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

        // Handle LINE:NOTIF|DATA: wrapped messages (all BLE notifications are wrapped)
        var wrappedData = '';
        var notifPrefix = 'LINE:NOTIF|DATA:';
        if (lineMatch && lineMatch[1] === 'NOTIF') {
            var dataIdx = message.indexOf('|DATA:');
            if (dataIdx !== -1) {
                wrappedData = message.slice(dataIdx + 6);
                // Re-parse regex against the unwrapped payload
                lineMatch = wrappedData.match(lineRegex);
                textMatch = wrappedData.match(textRegex);
                statusMatch = wrappedData.match(statusRegex);
            } else {
                return;
            }
        }

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

		// Handle Screen Mirror (LINE:S)
		if (lineMatch && lineMatch[1] === 'S') {
			var syncData = message.slice(lineMatch[0].length);
			logMessage('SCREEN:MIRROR data=' + JSON.stringify(syncData).substring(0, 200));
			var fields = app.parseScreenMirrorFields(syncData);
			logMessage('SCREEN:parsed fields=' + JSON.stringify(fields).substring(0, 300));
			app.renderScreenMirror(syncData);
			return;
		}

		if (lineMatch && textMatch) {
			var lineNumber = parseInt(lineMatch[1], 10);
			var text = textMatch ? textMatch[1] : '';
			
			if(lineNumber<11) {
				// Fallback: update legacy line-based screen display for older firmware
				var oldScreen = document.getElementById('screen');
				if (oldScreen) {
					var lineEl = document.getElementById('line' + lineNumber);
					if (lineEl) lineEl.textContent = text;
					oldScreen.classList.remove('empty-hint');
				}
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
            var pid = app.connectedDevices[target].peripheralId;
            if (!pid) return;
            ble.write(
                pid,
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
            var names = Object.keys(app.connectedDevices);
            var mac = names.length ? names[0].replace(/^LilygoT-Echo-/i, '').substr(-4) : '';
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
            var pid = app.connectedDevices[target].peripheralId;
            if (!pid) return;
            ble.write(
                pid,
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
        app.pttBleAlive = bleAlive;
        app.pttLoraAlive = loraAlive;
        
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
        } else if (!app.pttIsCapturing) {
            pttBtn.disabled = false;
            pttBtn.className = 'active';
            pttLabel.textContent = 'HOLD TO TALK';
        }
    },
    updatePttTransmitState: function(isActive) {
        var pttBtn = document.getElementById('pttButton');
        var pttLabel = document.getElementById('pttLabel');
        
        if (isActive) {
            app.pttIsCapturing = true;
            pttBtn.className = 'transmitting';
            pttLabel.textContent = 'TRANSMITTING...';
        } else {
            app.pttIsCapturing = false;
            pttLabel.textContent = 'SENT';
            setTimeout(function() {
                if (app.pttLoraAlive && app.pttBleAlive) {
                    pttBtn.className = 'active';
                    pttLabel.textContent = 'HOLD TO TALK';
                }
            }, 800);
        }
    },
    
    // Start mic capture + init Opus encoder
    startPttCapture: function() {
        if (app.pttIsCapturing) return;
        
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
                if (!app.pttIsCapturing) return;
                
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
                        if (opusBytes && app.pttIsCapturing) {
                            app.sendOpusFrame(opusBytes);
                        }
                    });
                }
            };
            
            app.pttCaptureInterval = opusInterval;
        }
    },
    
    stopPttCapture: function() {
        if (app.pttIsCapturing) {
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
    },

    // === Screen Mirror Rendering ===

    _screenMirrorData: {},

    parseScreenMirrorFields: function(data) {
        var fields = {};
        // Split on '|' to get sections: H:key,val C:key,val... S:key T:key G:key B:key I:key
        var parts = data.split('|');
        for (var p = 0; p < parts.length; p++) {
            var colonIdx = parts[p].indexOf(':');
            if (colonIdx === -1) continue;
            var sectionKey = parts[p][0]; // H, C, S, T, G, B, I
            var keyVal = parts[p].slice(colonIdx + 1);
            var entries = keyVal.split(',');
            for (var e = 0; e < entries.length; e++) {
                var entry = entries[e];
                if (!entry) continue;
                
                // Content section (C:) uses both ':' and '=' as separators in firmware.
                // Split on the first occurrence of either to get key/value.
                // Values may contain colons (time: 14:32) so we only split the first one.
                var sepIdx = entry.indexOf('=');
                var colonPos = entry.indexOf(':');
                
                if (sepIdx === -1 && colonPos === -1) continue;
                
                // Use whichever comes first, but handle edge cases:
                // Status bar entries like "B:90%" use the section-key colon already consumed.
                // Content fields like "range_role:Sender" or "r0_n=CNName" use = or : as KV separator.
                if (sectionKey === 'C') {
                    // Content fields: firmware sends both key:value and key=value.
                    // If there's a '=', split on first '='. Otherwise split on first ':'.
                    var kvSep = -1;
                    if (sepIdx !== -1) {
                        kvSep = sepIdx;
                    } else if (colonPos !== -1) {
                        kvSep = colonPos;
                    }
                    if (kvSep === -1) continue;
                    fields[entry.slice(0, kvSep)] = entry.slice(kvSep + 1);
                } else {
                    // Non-content sections: key is already split by section key.
                    // These entries use '=' as separator consistently in firmware.
                    if (sepIdx === -1) continue;
                    var k = entry.slice(0, sepIdx);
                    var v = entry.slice(sepIdx + 1);
                    fields[sectionKey.toLowerCase() + ':' + k] = v;
                }
            }
        }
        return fields;
    },

    renderScreenMirror: function(syncData) {
        if (!syncData) return;
        
        var fields = this.parseScreenMirrorFields(syncData);
        this._screenMirrorData = fields;

        var modeBadge = document.getElementById('screenModeBadge');
        var modeNameEl = document.getElementById('screenModeName');
        var chanSfEl = document.getElementById('screenChanSf');
        var contentArea = document.getElementById('screenContentArea');

        if (!contentArea) return;

        // Update header
        var mode = fields.h || 'TXT';
        if (modeBadge) modeBadge.textContent = mode;
        if (modeNameEl) modeNameEl.textContent = mode;
        if (chanSfEl) chanSfEl.textContent = fields['h:chansf'] || 'chn:A spf:7';

        // Update status bar
        var freqEl = document.getElementById('screenFreq');
        var timeEl = document.getElementById('screenTime');
        var satsEl = document.getElementById('screenSats');
        var batEl = document.getElementById('screenBatIcon');
        var gpsEl = document.getElementById('screenGpsIcon');

        if (freqEl) freqEl.textContent = fields['s:f'] || '---';
        if (timeEl) timeEl.textContent = fields['t:t'] || '--:--';
        if (satsEl) satsEl.textContent = fields['g:g'] || '0';

        // Battery icon (7 states: 0=empty through 6=full)
        var batIdx = parseInt(fields['i:batt']) || 6;
        if (batEl) {
            batEl.innerHTML = this._getBatIconSVG(batIdx);
        }

        // GPS icon
        var gpsState = fields['i:gpst'] || 'ok';
        if (gpsEl) {
            if (gpsState === 'no%') {
                gpsEl.innerHTML = '<svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="#F5A623" stroke-width="2"><circle cx="12" cy="12" r="10"/><line x1="12" y1="8" x2="12" y2="12"/><line x1="12" y1="16" x2="12.01" y2="16"/></svg>';
            } else {
                gpsEl.innerHTML = '<svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="#4ADE80" stroke-width="2"><path d="M21 10c0 7-9 13-9 13s-9-6-9-13a9 9 0 0118 0z"/><circle cx="12" cy="10" r="3"/></svg>';
            }
        }
        var modeIconEl = document.getElementById('screenModeIcon');
        if (modeIconEl) {
            modeIconEl.innerHTML = this._getModeIconSVG(mode);
        }

        // Render content rows based on mode
        var html = '';
        var c = fields;

        switch (mode) {
        case 'BEACON':
            var peerName = c['peer_name'] || '---';
            var peerDist = c['peer_dist'];
            if (peerDist && peerDist !== '-1') {
                html += this._screenRow(peerName);
                html += this._screenRow(peerDist + (peerDist.indexOf('km') === -1 ? ' m' : ''));
            } else {
                html += this._screenRow('No peer loc');
                html += this._screenRow('???');
            }
            var rosterCount = c['roster_count'] || 0;
            html += this._screenRow(rosterCount + ' peers');
            for (var r = 0; r < 8; r++) {
                var rn = c['r' + r + '_n'] || '';
                var rd = c['r' + r + '_d'] || '';
                var rb = c['r' + r + '_b'] || '';
                if (rn) {
                    html += this._screenRow(rn + ' ' + rd + ' ' + rb);
                } else {
                    html += '<div class="screen-row blank">&nbsp;</div>';
                }
            }
            break;

        case 'RANGE':
            var role = c['range_role'] || '---';
            html += this._screenRow('Role: ' + role);
            if (c['curr_lat']) {
                html += this._screenRow(c['curr_lat'] + ', ' + c['curr_lon']);
            }
            html += this._screenRow('Rng:' + (c['range_last_count'] || '---'));
            html += this._screenRow('Stable: ' + c['range_stable_dist'] + ' m');
            html += this._screenRow('PLoss: ' + c['range_total_loss'] + '/' + c['range_consecutive_ok']);
            break;

        case 'PTT':
            var pttState = c['ptt_state'] || 'Idle';
            if (pttState === 'TX') {
                html += this._screenRow('<span class="screen-tx">TX ---</span>');
            } else if (pttState === 'RX') {
                html += this._screenRow('<span class="screen-rx">RX ---</span>');
            } else {
                html += this._screenRow('PTT Idle');
            }
            break;

        case 'TXT':
            var msgCount = parseInt(c['txt_inbox_count']) || 0;
            if (!c['txt_show_inbox'] && c['txt_latest_msg']) {
                html += this._screenRow(msgCount + ' msg(s)');
                html += this._screenRow('---');
                // Truncate long message for screen width
                var maxLineLen = 42;
                var msg = c['txt_latest_msg'];
                if (msg.length > maxLineLen) msg = msg.slice(0, maxLineLen - 3) + '...';
                html += this._screenRow(msg);
            } else if (c['txt_show_inbox'] === '1') {
                var scrollPage = parseInt(c['txt_scroll_page']) || 0;
                html += this._screenRow('Inbox: P ' + (scrollPage + 1) + '/' + Math.ceil(Math.max(msgCount, 1) / 16));
                for (var rp = 0; rp < 8; rp++) {
                    // App would need to know the inbox content — just show page info
                    html += this._screenRow('...');
                }
            } else {
                html += this._screenRow('No messages yet');
                html += this._screenRow('Wait for TXT mode');
            }
            break;

        case 'TST':
            html += this._screenRow('Sent: ' + (c['tst_sent'] || '---'));
            html += this._screenRow('Recv: ' + (c['tst_recv'] || '---'));
            break;

        case 'PONG':
            var pongState = parseInt(c['pong_state']) || 0;
            var stateText = ['Idle', 'Sending', 'Receive'][pongState] || '???';
            html += this._screenRow('State: ' + stateText);
            html += this._screenRow('RTT: ' + (c['pong_rtt_ms'] || '---') + ' ms');
            break;

        case 'SCAN':
            var freq = c['scan_freq'] || '--- MHz';
            var progress = c['scan_progress'] || '-%';
            html += this._screenRow('Freq: ' + freq);
            html += this._screenRow('Progress: ' + progress);
            // Scan results rows
            for (var sr = 0; sr < 10; sr++) {
                var sf = c['s' + sr + '_f'] || '';
                var rs = c['s' + sr + '_r'] || '';
                if (sf && sf !== '---mh') {
                    html += this._screenRow(sf.replace('mh',' MHz') + ' R' + rs.replace('dB',''));
                } else {
                    html += '<div class="screen-row blank">&nbsp;</div>';
                }
            }
            break;

        case 'RAW':
            html += this._screenRow('Rcv Cnt: ' + (c['raw_count'] || '---'));
            html += this._screenRow('SNR: ' + c['raw_last_snr'] + ' dB');
            html += this._screenRow('RSSI: ' + c['raw_last_rssi'] + ' dBm');
            break;

        case 'WP':
            var wpLabel = c['wp_label'] || '---';
            html += this._screenRow(wpLabel);
            html += this._screenRow(c['wp_lat'] + ', ' + c['wp_lon']);
            html += this._screenRow('Alt: ' + c['wp_alt'] + ' m');
            if (c['wp_broadcasting'] === '1') {
                html += this._screenRow('Broadcasting: ' + c['wp_bcast_rem'] + 's');
            }
            break;

        default:
            html += this._screenRow('Mode: ' + mode);
            break;
        }

        contentArea.innerHTML = html;
    },

    _screenRow: function(text) {
        return '<div class="screen-row">' + (text || '&nbsp;') + '</div>';
    },

    _getBatIconSVG: function(level) {
        // 7 battery levels as inline SVG
        var fill = Math.max(1, level); // filled bars
        var totalBars = 5;
        var bars = '';
        for (var i = 0; i < totalBars; i++) {
            var color = i < fill ? '#4ADE80' : '#3D5A73';
            bars += '<rect x="' + (16 + i * 5) + '" y="2" width="3" height="12" rx="0.5" fill="' + color + '"/>';
        }
        return '<svg width="18" height="16" viewBox="0 0 32 16">' + bars +
               '<rect x="14" y="0" width="4" height="2" rx="0.5" fill="#3D5A73"/>' +
               '</svg>';
    },

    _getModeIconSVG: function(mode) {
        var icons = {
            'BEACON': '<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="#C8DCE8" stroke-width="2"><circle cx="12" cy="12" r="3"/><path d="M12 1v4m0 14v4m-9.5-9.5l3 3m8 8l3 3M5.6 5.6l2.8 2.8m7.2 7.2l2.8 2.8"/></svg>',
            'RAW': '<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="#C8DCE8" stroke-width="2"><polyline points="4 7 4 4 20 4 20 7"/><line x1="9" y1="20" x2="15" y2="20"/><line x1="12" y1="4" x2="12" y2="20"/></svg>',
            'TXT': '<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="#C8DCE8" stroke-width="2"><path d="M14 2H6a2 2 0 00-2 2v16a2 2 0 002 2h12a2 2 0 002-2V8z"/><polyline points="14 2 14 8 20 8"/></svg>',
            'RANGE': '<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="#C8DCE8" stroke-width="2"><circle cx="12" cy="12" r="10"/><path d="M2 12h20m-4-4l4 4-4 4"/></svg>',
            'TST': '<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="#C8DCE8" stroke-width="2"><path d="M22 12h-4l-3 9L9 3l-3 9H2"/></svg>',
            'PONG': '<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="#C8DCE8" stroke-width="2"><circle cx="12" cy="12" r="10"/><line x1="12" y1="8" x2="12" y2="16"/><line x1="8" y1="12" x2="16" y2="12"/></svg>',
            'SCAN': '<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="#C8DCE8" stroke-width="2"><polyline points="22 12 18 12 15 21 9 3 6 12 2 12"/></svg>',
            'PTT': '<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="#C8DCE8" stroke-width="2"><rect x="9" y="2" width="6" height="11" rx="3"/><path d="M5 15v1a3 3 0 003 3h8a3 3 0 003-3v-1"/></svg>',
            'WP': '<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="#C8DCE8" stroke-width="2"><path d="M21 10c0 7-9 13-9 13s-9-6-9-13a9 9 0 0118 0z"/><circle cx="12" cy="10" r="3"/></svg>',
        };
        return icons[mode] || icons['TXT'];
    }
};
