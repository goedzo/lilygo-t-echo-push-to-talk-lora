#include "delay.h"
#include "utilities.h"
#include <SPI.h>
#include <GxEPD.h>
#include <GxDEPG0150BN/GxDEPG0150BN.h>  // 1.54" b/w
#include <RadioLib.h>
#include <stdint.h>
#include "settings.h"
#include "display.h"
#include "app_modes.h"
#include "lora.h"
#include "packet.h"
#include "gps.h"
#include "battery.h"
#include "buddy_list.h"
#include <time.h>  // For RTC time management
#include <stdlib.h>  // For random number generation

#define SEND_PACKET_QUEUE_SIZE 10

// NAK reliability globals — initialized in initNakReliability()
PendingReq outstandingReqs[RETRANSMIT_BUFFER_SIZE];

ReqDedupEntry recentReqs[8];

unsigned long lastReqSendTime = 0;  // millis() of last outgoing REQ transmission

bool nakInitialized = false;

// Track counters we've already received — used to resolve outstanding REQs
unsigned int packetsReceivedSinceLastCheck = 0;

PacketQueue receivePacketQueue[RECEIVE_PACKET_QUEUE_SIZE];
int receivePacketQueueCount = 0;

// Fixed-size retransmit buffer (no heap allocation)
PacketBuffer retransmitPacketBuffer[RETRANSMIT_BUFFER_SIZE];
int bufferIndex = 0;

PacketQueueEntry packetQueue[SEND_PACKET_QUEUE_SIZE];  // Queue for pending packets
int queueHead = 0;  // Points to the head of the queue
int queueTail = 0;  // Points to the tail of the queue
bool transmitInProgress = false;  // Track transmission state




SX1262* radio = nullptr;
SPIClass* rfPort = nullptr;

// Discovery / hopping constants
#define DISCOVERY_FREQUENCY      869.47   // Known discovery frequency (MHz) — used for probe-based discovery and periodic resync
#define DEFAULT_FREQUENCY        869.47   // Default frequency when no hopping or map is available
#define DISCOVERY_PERIOD         300      // Return to discovery channel every 5 minutes (seconds)
#define PROBEBEACON_JITTER_MIN   40       // Minimum seconds into hop interval before transmitting PRB beacon (synced mode only)
#define PROBEBEACON_JITTER_MAX   47       // Maximum seconds into hop interval before transmitting PRB beacon (synced mode only)

float defaultFrequency = DEFAULT_FREQUENCY;  // Default frequency
float currentFrequency = defaultFrequency;

// Frequency hopping related variables
float startFreq = 863.0;
float endFreq = 869.65;
float stepSize = 0.01;
int numFrequencies = (endFreq - startFreq) / stepSize;
float* frequencyMap = nullptr;
int FrequencyHopSeconds = 47; //After how many seconds do we hop to the next frequency?

// All channels are always used — no bad-channel tracking.
// Deterministic hopping depends on all devices selecting the same channel each cycle.

// Epoch-aligned hop interval computation — seconds in a day for cycling
#define HOP_EPOCH_SECONDS 86400UL

// Time convergence parameters — allows devices within ±hop_interval to agree on phase
#define TIME_CONVERGENCE_TOLERANCE 5  // Seconds tolerance for gradual time sync
#define TIME_CONVERGENCE_MAX_JUMP 3   // Max seconds to adjust per packet (prevents large jumps)

volatile bool operationDone = false;  // Flag to indicate radio operation is done
bool transmitFlag = false;            // Flag for transmission state
size_t timeOnAir = 0;                 // Time-on-air for transmitted packets

bool hopAfterTxRx = false;            // Hop when our last action was completed
float hopToFrequency;

unsigned long lastHopTime = 0;  // Time of last hop

unsigned long syncLossTimer = 0;  // Timer for detecting lost synchronization

unsigned long resendTime = 0;  // Non-blocking timer for post-hop retransmit delay

unsigned long lastPeerPacketTime = 0;  // Track when last packet from peer was received
bool     peerPacketReceived = false;    // Guard against zero-boot artifact
unsigned long lastBeaconTime = 0;      // Track when last beacon was sent

// Probe-based frequency hopping discovery globals
bool inProbeMode = true;              // Start in probe mode — waiting for time sync
unsigned long firstBootMillis = 0;    // Boot timestamp used for probe timing
bool heardProbeThisCycle = false;     // Whether we received a PRB during current hop cycle
unsigned long syncLockUntilCycle = 0; // Hop cycle number until which we lock to discovery freq after PRB receive

// Peer-to-peer handshake (bidirectional liveness)
bool peerAcked = false;
unsigned long peerAckedTimeMs = 0;

void setPeerAcked() {
    peerAcked = true;
    peerAckedTimeMs = millis();
}

bool isPeerAlive() {
    if (!peerPacketReceived && !peerAcked) return false;  // Never report alive before first packet or ACK
    bool received_ok = (millis() - lastPeerPacketTime < PEER_TIMEOUT);
    bool acked_ok = peerAcked && (millis() - peerAckedTimeMs < PEER_TIMEOUT);
    return received_ok || acked_ok;  // Alive if either direction confirmed
}

// Peer roster globals for BEACON mode
PeerEntry peerRoster[MAX_ROSTER_PEERS];
int     peerRosterCount = 0;
bool    inBeaconMode = false;

void beaconAddOrUpdate(const Packet& packet) {
    // Extract device ID from B prefix: "B{id_short}"
    String devId = packet.beacon_deviceId;
    if (devId.length() == 0 || devId == bleGetDeviceIdShort()) return; // Skip self
    
    unsigned long now = millis();
    
    // Check if peer already in roster — update it
    for (int i = 0; i < peerRosterCount; i++) {
        if (peerRoster[i].deviceId == devId) {
            peerRoster[i].lat     = packet.beacon_lat;
            peerRoster[i].lon     = packet.beacon_lon;
            peerRoster[i].battery = packet.beacon_battery;
            strncpy(peerRoster[i].callSign, packet.beacon_callSign, 16);
            peerRoster[i].callSign[16] = '\0';
            if (packet.beacon_callSign[0] != '\0') {
                buddyAddOrUpdate(devId.c_str(), packet.beacon_callSign);
            }
            peerRoster[i].lastSeen = now;
            
            // Compute distance if we have GPS fix
            if (gps_status == GPS_LOC) {
                peerRoster[i].distanceM = TinyGPSPlus::distanceBetween(
                    gps_latitude, gps_longitude,
                    packet.beacon_lat, packet.beacon_lon);
            }
            return;
        }
    }
    
    // Add new entry — find empty slot
    if (peerRosterCount >= MAX_ROSTER_PEERS) {
        // Roster full — discard oldest (lowest index is oldest)
        peerRosterCount--;
        for (int i = 0; i < peerRosterCount; i++) {
            peerRoster[i] = peerRoster[i + 1];
        }
    }
    
    int slot = peerRosterCount++;
    peerRoster[slot].deviceId  = devId;
    strncpy(peerRoster[slot].callSign, packet.beacon_callSign, 16);
    peerRoster[slot].callSign[16] = '\0';
    if (packet.beacon_callSign[0] != '\0') {
        buddyAddOrUpdate(devId.c_str(), packet.beacon_callSign);
    }
    peerRoster[slot].lat       = packet.beacon_lat;
    peerRoster[slot].lon       = packet.beacon_lon;
    peerRoster[slot].battery   = packet.beacon_battery;
    peerRoster[slot].lastSeen  = now;
    peerRoster[slot].distanceM = 0;
    
    if (gps_status == GPS_LOC) {
        peerRoster[slot].distanceM = TinyGPSPlus::distanceBetween(
            gps_latitude, gps_longitude,
            packet.beacon_lat, packet.beacon_lon);
    }

    // Trigger screen sync if BEACON mode is active
    extern const char* current_mode;
    extern void markScreenDirty();
    if (strcmp(current_mode, "BEACON") == 0) {
        markScreenDirty();
    }
}

void beaconDisplayRoster(uint8_t line) {
    char buf[30];
    unsigned long now = millis();
    
    // Clean stale entries (beyond PEER_TIMEOUT)
    int activeCount = 0;
    for (int i = 0; i < peerRosterCount; i++) {
        if (now - peerRoster[i].lastSeen > PEER_TIMEOUT) {
            peerRoster[i] = peerRoster[peerRosterCount - 1];
            peerRosterCount--;
            i--; // recheck this index
        } else {
            activeCount++;
        }
    }
    
    updDisp(line, "Peers:", true);
    
    int maxLines = (disp_height + disp_top_margin) / disp_font_height;
    for (int i = 0; i < activeCount && (line + 1 + i) < maxLines; i++) {
        float dist = peerRoster[i].distanceM;
        const char* displayName = peerRoster[i].callSign[0] != '\0' ? peerRoster[i].callSign : peerRoster[i].deviceId.c_str();
        if (dist > 0 && dist < 1000) {
            snprintf(buf, sizeof(buf), "%s %.0fm", displayName, dist);
        } else if (dist >= 1000) {
            snprintf(buf, sizeof(buf), "%s %.1fkm", displayName, dist / 1000.0);
        } else {
            snprintf(buf, sizeof(buf), "%s ???m", displayName);
        }
        // Add battery indicator
        int batt = peerRoster[i].battery;
        if (batt > 0) {
            strncat(buf, " ", sizeof(buf) - strlen(buf) - 1);
            char battBuf[12];
            snprintf(battBuf, sizeof(battBuf), "%d%%", batt);
            strncat(buf, battBuf, sizeof(buf) - strlen(buf) - 1);
        } else {
            strncat(buf, " -%", sizeof(buf) - strlen(buf) - 1);
        }
        
        char displayLine[5];
        snprintf(displayLine, sizeof(displayLine), "%d", line + 1 + i);
        // Use direct line number
        updDisp(line + 1 + i, buf, true);
    }
}

void sendPeerBeacon() {
    extern const char* buddyGetDisplayName();
    const char* myCallSign = buddyGetDisplayName();
    static char beacon[MAX_PACKET_SIZE];
    
    if (gps_status == GPS_LOC) {
        int batt = getBatteryPercentage();
        if (myCallSign[0] != '\0') {
            snprintf(beacon, sizeof(beacon), "B%s~CN%s~GP%.6f,%.6f~BT%d", bleGetDeviceIdShort(), myCallSign, gps_latitude, gps_longitude, batt);
        } else {
            snprintf(beacon, sizeof(beacon), "B%s~GP%.6f,%.6f~BT%d", bleGetDeviceIdShort(), gps_latitude, gps_longitude, batt);
        }
    } else {
        int batt = getBatteryPercentage();
        if (myCallSign[0] != '\0') {
            snprintf(beacon, sizeof(beacon), "B%s~CN%s~BT%d", bleGetDeviceIdShort(), myCallSign, batt);
        } else {
            snprintf(beacon, sizeof(beacon), "B%s~BT%d", bleGetDeviceIdShort(), batt);
        }
    }
    sendPacket((uint8_t*)beacon, strlen(beacon));
}

unsigned int messageCounter = 0;       // The counter I add to each message so that it can be tracket
unsigned int lastReceivedCounter = 0;  // Global variable to store the last received packet counter
uint8_t lastMessageBuffer[MAX_PACKET_SIZE];  // Fixed-size buffer for last message
uint16_t lastMessageLength = 0;        // Length of the last message sent
unsigned int lastMessageCounter=0;     // The packetCounter of the last message

// Move numFrequencies and frequency calculations to initialization
void initializeFrequencyMap() {
    numFrequencies = (endFreq - startFreq) / stepSize;

    frequencyMap = new float[numFrequencies];
    for (int i = 0; i < numFrequencies; i++) {
        frequencyMap[i] = startFreq + (i * stepSize);
    }
}

// Pseudo-random frequency hopping based on a shared time source and randomness
float getNextFrequency(unsigned long sharedTime, unsigned long sharedSeed) {
    // Use epoch-aligned interval count so devices within ±hop_interval agree on phase
    unsigned long totalSeconds = sharedTime % HOP_EPOCH_SECONDS;
    unsigned long intervalCount = totalSeconds / FrequencyHopSeconds;

    // Combine intervalCount and sharedSeed for randomness
    unsigned long randomValue = intervalCount ^ sharedSeed;  // XOR interval count with the seed for randomness

    // Generate a random starting index based on intervalCount
    int startingIndex = randomValue % numFrequencies;

    // Always return the channel at this index — all channels used unconditionally.
    int index = (startingIndex + 0) % numFrequencies;
    return frequencyMap[index];
}


// Simple checksum calculation for validating received maps
unsigned char calculateChecksum(const unsigned char* data, int len) {
    unsigned char checksum = 0;
    for (int i = 0; i < len; i++) {
        checksum ^= data[i];  // XOR all bytes
    }
    return checksum;
}

// Extract the ~SD (Send DateTime) field value from a raw packet buffer
// Returns true if found and copies into outDateTime (max 14 chars + null)
bool extractSDField(const uint8_t* buffer, uint16_t bufferSize, String& outDateTime) {
    // Look for ~SD in the header portion (everything before ~~)
    uint16_t idx = 0;
    while (idx < bufferSize && buffer[idx] != '~') idx++;  // Skip past packet type
    if (idx >= bufferSize) return false;
    
    idx++;  // skip first ~
    outDateTime = "";
    while (idx + 1 < bufferSize) {
        if (buffer[idx] == '~' && buffer[idx + 1] == '~') break;  // End of headers
        
        // Look for ~SD field
        if (buffer[idx] == '~') {
            idx++;  // skip ~
            if (idx + 1 < bufferSize && buffer[idx] == 'S' && buffer[idx + 1] == 'D') {
                // Found ~SD — read until next ~ or ~~
                idx += 2;  // skip SD
                while (idx < bufferSize && buffer[idx] != '~') {
                    outDateTime += (char)buffer[idx];
                    idx++;
                }
                if (outDateTime.length() == 14) return true;
            }
        } else {
            // Skip to next ~ or ~~
            while (idx < bufferSize && buffer[idx] != '~') idx++;
        }
    }
    return false;
}

// Automatically sync local RTC from received packet timestamp when GPS has
// not yet provided a fix — attempts to use the parsed sendDateTime field,
// falling back to raw SD field extraction if the packet type lacks it.
void autoSyncRTCFromPacket(const Packet& packet) {
    if (!packet.sendDateTime.length()) {
        // Try raw extraction as fallback (for non-standard packet types)
        String sdVal;
        if (extractSDField(packet.raw, packet.rawLength, sdVal)) {
            adjustRTC(sdVal);
        }
        return;
    }
    
    // Use the parsed sendDateTime field directly
    if (packet.sendDateTime.length() != 14) return;
    
    long receiverTimeSeconds = 0;
    receiverTimeSeconds = packet.sendDateTime.substring(8, 10).toInt() * 3600
                        + packet.sendDateTime.substring(10, 12).toInt() * 60
                        + packet.sendDateTime.substring(12, 14).toInt();
    
    RTC_Date rtc_time = rtc.getDateTime();
    long localSeconds = rtc_time.hour * 3600 + rtc_time.minute * 60 + rtc_time.second;
    long timeDiff = abs(receiverTimeSeconds - localSeconds);
    
    // If we don't have GPS time yet, accept the sender's time as truth
    if (!time_set) {
        adjustRTC(packet.sendDateTime);
    } 
    // If we DO have GPS time but drifting, use existing gradual convergence logic
    else if (timeDiff > TIME_CONVERGENCE_TOLERANCE) {
        long jump = constrain(receiverTimeSeconds - localSeconds, -TIME_CONVERGENCE_MAX_JUMP, TIME_CONVERGENCE_MAX_JUMP);
        RTC_Date currentRtc = rtc.getDateTime();
        long currentRtcSeconds = currentRtc.hour * 3600 + currentRtc.minute * 60 + currentRtc.second;
        long newRtcSeconds = currentRtcSeconds + jump;
        
        if (newRtcSeconds < 0) {
            newRtcSeconds += HOP_EPOCH_SECONDS;
        } else if (newRtcSeconds >= HOP_EPOCH_SECONDS) {
            newRtcSeconds -= HOP_EPOCH_SECONDS;
        }
        
        long hours = newRtcSeconds / 3600;
        long minutes = (newRtcSeconds % 3600) / 60;
        long seconds = newRtcSeconds % 60;
        
        rtc.setDateTime(currentRtc.year, currentRtc.month, currentRtc.day, hours, minutes, seconds);
    } else {
        // Within tolerance — just nudge toward sender's time
        long jump = constrain(receiverTimeSeconds - localSeconds, -1, 1);
        if (jump != 0) {
            RTC_Date currentRtc = rtc.getDateTime();
            long newSeconds = currentRtc.hour * 3600 + currentRtc.minute * 60 + currentRtc.second + jump;
            if (newSeconds < 0) newSeconds += HOP_EPOCH_SECONDS;
            if (newSeconds >= HOP_EPOCH_SECONDS) newSeconds -= HOP_EPOCH_SECONDS;
            long h = newSeconds / 3600;
            long m = (newSeconds % 3600) / 60;
            long s = newSeconds % 60;
            rtc.setDateTime(currentRtc.year, currentRtc.month, currentRtc.day, h, m, s);
        }
    }
}

// Send a probe beacon on the discovery frequency with RTC timestamp for sync
void sendProbeBeacon() {
    // Include ~SD field so receiver can adopt our RTC time
    char probeBuf[120];
    snprintf(probeBuf, sizeof(probeBuf), "PR~DI%s~SD%s", bleGetDeviceIdShort(), getFormattedDateTime().c_str());
    sendPacket(probeBuf);
}


void setFlag(void) {
    operationDone = true;
 }

void storePacketInQueue(uint8_t* pkt_buf, uint16_t len, unsigned int counter) {
    if (receivePacketQueueCount >= RECEIVE_PACKET_QUEUE_SIZE) {
        sendSerialToAppLn(F("Newer packet queue full, discarding packet."));
        return;
    }
    // Truncate to max size — no heap allocation needed
    uint16_t safeLen = len > MAX_PACKET_SIZE ? MAX_PACKET_SIZE : len;
    memcpy(receivePacketQueue[receivePacketQueueCount].packetData, pkt_buf, safeLen);
    receivePacketQueue[receivePacketQueueCount].packetLen = safeLen;
    receivePacketQueue[receivePacketQueueCount].packetCounter = counter;
    receivePacketQueueCount++;
}

void processPacketQueue() {
    // Sort the packets by packetCounter (ascending order)
    for (int i = 0; i < receivePacketQueueCount - 1; i++) {
        for (int j = 0; j < receivePacketQueueCount - i - 1; j++) {
            if (receivePacketQueue[j].packetCounter > receivePacketQueue[j + 1].packetCounter) {
                PacketQueue temp = receivePacketQueue[j];
                receivePacketQueue[j] = receivePacketQueue[j + 1];
                receivePacketQueue[j + 1] = temp;
            }
        }
    }

    // Now process the sorted packets
    for (int i = 0; i < receivePacketQueueCount; i++) {
        Packet packet;
        if (packet.parsePacket(receivePacketQueue[i].packetData, receivePacketQueue[i].packetLen)) {
            sendSerialToApp(F("Processing queued packet: "));
            sendSerialToAppLn((String)packet.packetCounter);
      
            // Only process this queue when we have the next packet available
            unsigned int expectedPacketCounter = lastReceivedCounter + 1;
            if (packet.packetCounter == expectedPacketCounter) {
                handlePacket(packet);  // Process the packet
                lastReceivedCounter = packet.packetCounter;
            }
        }
    }
    receivePacketQueueCount = 0;  // Reset the queue count after processing
}


bool checkForMissingPackets(Packet& packet, uint8_t* rcv_pkt_buf, uint16_t packet_len) {
    unsigned int expectedPacketCounter = lastReceivedCounter + 1;
    unsigned int missedPackets = packet.packetCounter - lastReceivedCounter - 1;

    if (lastReceivedCounter > 0 && missedPackets > 0) {
        // Check if we missed more than RETRANSMIT_BUFFER_SIZE packets
        if (missedPackets > RETRANSMIT_BUFFER_SIZE) {
            sendSerialToAppLn(F("Too many packets missed, unable to request older packets"));
            lastReceivedCounter = packet.packetCounter;  // Skip to the newest packet
            return true;  // Continue processing since no retransmission request is needed
        } else {
            // Request all missed packets with spacing and dedup tracking
            for (unsigned int missedCounter = expectedPacketCounter; missedCounter < packet.packetCounter; missedCounter++) {
                sendRetransmitRequest(missedCounter);
                sendSerialToApp(F("Requesting retransmission of packet: "));
                sendSerialToAppLn((String)missedCounter);

                char buf[50];
                snprintf(buf, sizeof(buf), "Missed: %d", missedCounter);
                showError(buf);

                // Space out REQ transmissions — LoRa airtime + processing time between each REQ
                unsigned long now = millis();
                unsigned long timeSinceLastReq = now - lastReqSendTime;
                if (timeSinceLastReq < REQ_PACKET_SPACING_MS) {
                    // Instead of blocking with delay(), we stop requesting more packets in this loop 
                    // and let the main loop handle other tasks. The remaining missed packets will be 
                    // requested once enough time has passed, or on next packet reception if logic allows.
                    // However, to maintain correctness without a state machine for REQs, 
                    // we'll simply break here; subsequent requests are handled by the fact that 
                    // this function is called when a new packet arrives and gaps are detected.
                    break; 
                }
            }
            // Store the newer packet in the queue until all missing packets are received
            storePacketInQueue(rcv_pkt_buf, packet_len, packet.packetCounter);

            return false;  // Halt the processing until missed packets are handled
        }
    } else {
        // No packets are missing, process the received packet normally
        lastReceivedCounter = packet.packetCounter;
        return true;  // Continue processing the packet
    }
}

void handleRetransmitRequestComplete() {
    sendSerialToAppLn(F("Retransmission request completed, processing queued packets..."));

    // Process all queued packets, in order, as long as they are in sequence
    bool packetProcessed = true;
    while (packetProcessed) {
        packetProcessed = false;
        for (int i = 0; i < receivePacketQueueCount; i++) {
            // Check if the next expected packet is in the queue
            if (receivePacketQueue[i].packetCounter == lastReceivedCounter + 1) {
                // Process the packet
                Packet packet;
                if (packet.parsePacket(receivePacketQueue[i].packetData, receivePacketQueue[i].packetLen)) {
                    sendSerialToApp(F("Processing queued packet: "));
                    sendSerialToAppLn((String)packet.packetCounter);

                    char buf[50];
                    snprintf(buf, sizeof(buf), "Recover: %d", packet.packetCounter);
                    showError(buf);

                    handlePacket(packet);  // Process the packet

                    // Update the last received counter
                    lastReceivedCounter = packet.packetCounter;
                    packetProcessed = true;
                }

                // Shift the remaining packets in the queue
                for (int j = i; j < receivePacketQueueCount - 1; j++) {
                    receivePacketQueue[j] = receivePacketQueue[j + 1];
                }
                receivePacketQueueCount--;

                // Break to recheck the queue after processing
                break;
            }
        }
    }
    //Clear the error
    showError("");

}



// Modified checkLoraPacketComplete to avoid redundant sharedTime calculation
void checkLoraPacketComplete() {
    if (operationDone) {
        operationDone = false;

        if (transmitFlag) {
            transmitFlag = false;
            int state = radio->finishTransmit();
            if (state == RADIOLIB_ERR_NONE) {
                sendSerialToApp(F("Packet was Sent, finishTransmit"));
            } else {
                sendSerialToApp(F("Sent failed, code "));
                sendSerialToAppLn((String)state);
            }
            if (hopAfterTxRx) {
                hopAfterTxRx = false;
                setFrequency(hopToFrequency);  // Set the new frequency
                if (lastMessageLength > 0) {
                    resendTime = millis() + 1000;  // Defer retransmit — non-blocking
                }
            } else {
                radio->startReceive();  // Start receiving after transmission
            }
            //If there are more messages in the queue, send them now
            handleTransmissionComplete();

            // Non-blocking post-hop retransmit — defer until resendTime
            if (resendTime > 0 && millis() >= resendTime) {
                sendSerialToAppLn(F("Resending last message after frequency hop"));
                sendPacket(lastMessageBuffer, lastMessageLength, lastMessageCounter);
                resendTime = 0;
            }
        } else {
            uint16_t packet_len = radio->getPacketLength(false);
            uint16_t irqFlags = radio->getIrqFlags();
            unsigned char rcv_pkt_buf[MAX_PKT];

            if (irqFlags & RADIOLIB_SX126X_IRQ_RX_DONE) {
                memset(rcv_pkt_buf, 0, MAX_PKT);  // Clear the receive buffer
                int state = radio->readData(rcv_pkt_buf, packet_len);
                radio->startReceive();  // Quickly continue receiving
                if (state == RADIOLIB_ERR_NONE) {
                    rcv_pkt_buf[packet_len] = '\0';  // Null-terminate the received packet

                    Packet packet;
                    if (packet.parsePacket(rcv_pkt_buf, packet_len)) {
                        sendSerialToApp(F("Packet parsed: "));
                        sendSerialToApp((String)packet.type);
                        sendSerialToApp("\n");
                        sendSerialToApp(packet.content);
                        sendSerialToApp("\n");
                        sendSerialToApp(F("PC: "));
                        sendSerialToAppLn((String)packet.packetCounter);

                        // Extract sender's time from the packet for gradual convergence
                        long receiverTimeSeconds = 0;
                        if (packet.sendDateTime.length() == 14) {
                            receiverTimeSeconds = packet.sendDateTime.substring(8, 10).toInt() * 3600
                                                + packet.sendDateTime.substring(10, 12).toInt() * 60
                                                + packet.sendDateTime.substring(12, 14).toInt();
                        }

                        RTC_Date rtc_time = rtc.getDateTime();
                        long localSeconds = rtc_time.hour * 3600 + rtc_time.minute * 60 + rtc_time.second;

                        // Check if time is out of sync and converge gradually
                        long timeDiff = abs(receiverTimeSeconds - localSeconds);
                        if (timeDiff > TIME_CONVERGENCE_TOLERANCE) {
                            long jump = constrain(receiverTimeSeconds - localSeconds, -TIME_CONVERGENCE_MAX_JUMP, TIME_CONVERGENCE_MAX_JUMP);
                            RTC_Date currentRtc = rtc.getDateTime();
                            long currentRtcSeconds = currentRtc.hour * 3600 + currentRtc.minute * 60 + currentRtc.second;
                            long newRtcSeconds = currentRtcSeconds + jump;

                            // Handle day wraparound
                            if (newRtcSeconds < 0) {
                                newRtcSeconds += HOP_EPOCH_SECONDS;
                            } else if (newRtcSeconds >= HOP_EPOCH_SECONDS) {
                                newRtcSeconds -= HOP_EPOCH_SECONDS;
                            }

                            long hours = newRtcSeconds / 3600;
                            long minutes = (newRtcSeconds % 3600) / 60;
                            long seconds = newRtcSeconds % 60;

                            rtc.setDateTime(currentRtc.year, currentRtc.month, currentRtc.day, hours, minutes, seconds);
                        } else {
                            // Within tolerance — just nudge toward sender's time
                            long jump = constrain(receiverTimeSeconds - localSeconds, -1, 1);
                            if (jump != 0) {
                                RTC_Date currentRtc = rtc.getDateTime();
                                long newSeconds = currentRtc.hour * 3600 + currentRtc.minute * 60 + currentRtc.second + jump;
                                if (newSeconds < 0) newSeconds += HOP_EPOCH_SECONDS;
                                if (newSeconds >= HOP_EPOCH_SECONDS) newSeconds -= HOP_EPOCH_SECONDS;
                                long h = newSeconds / 3600;
                                long m = (newSeconds % 3600) / 60;
                                long s = newSeconds % 60;
                                rtc.setDateTime(currentRtc.year, currentRtc.month, currentRtc.day, h, m, s);
                             }
                         }

                        // PRB packet handling — probe discovery: extract DI and auto-sync RTC
                        if (packet.type == "PRB") {
                            sendSerialToApp(F("PRB rx on "));
                            sendSerialToApp((String)currentFrequency);
                            sendSerialToAppLn(F(" MHz"));

                            bool wasInProbe = inProbeMode;  // Save before any changes below

                            if (inProbeMode) {
                                inProbeMode = false;  // Exit probe mode — we can hop now
                                sendSerialToAppLn(F("→ exited PROBE mode, starting hopping"));
                            } else {
                                sendSerialToAppLn(F("→ periodic resync PRB"));
                            }

                            heardProbeThisCycle = true;  // Mark that we heard a probe this cycle

                            // Lock to discovery channel for several hop cycles after PRB receive
                            // Use sender's time (from ~SD) so both devices start their lock period together.
                            unsigned long senderCycle = 0;
                            if (packet.sendDateTime.length() >= 14) {
                                unsigned long senderSecs = packet.sendDateTime.substring(8, 10).toInt() * 3600
                                                 + packet.sendDateTime.substring(10, 12).toInt() * 60
                                                 + packet.sendDateTime.substring(12, 14).toInt();
                                senderCycle = (senderSecs / FrequencyHopSeconds) + 5;
                            }

                            // After syncing, set lock period based on sender's cycle so both devices diverge together
                            if (senderCycle > 0) {
                                syncLockUntilCycle = senderCycle;
                            }

                            // Always accept peer time on first PRB after probe exit — GPS may be stale.
                            if (!time_set || wasInProbe) {
                                time_set = true;
                                sendSerialToApp(F("RTC synced from PRB ~SD: "));
                                sendSerialToAppLn(packet.sendDateTime);
                                adjustRTC(packet.sendDateTime);
                            } else if (packet.sendDateTime.length() >= 14) {
                                // Already peer-synced — gradual convergence only
                             autoSyncRTCFromPacket(packet);
                         }
                         
                         // Extract sender's device ID from ~DI field for logging and use in sync confirmation
                         String senderID;
                         const char* rawPtr = (const char*)rcv_pkt_buf;
                         uint16_t pi = 0;
                         while (pi < packet_len && rcv_pkt_buf[pi] != '~') pi++;
                         if (pi < packet_len) {
                             pi++; // skip first ~
                             while (pi + 1 < packet_len) {
                                 if (rcv_pkt_buf[pi] == '~' && rcv_pkt_buf[pi+1] == '~') break;
                                 if (rcv_pkt_buf[pi] == '~') {
                                     pi++;
                                     if (pi + 2 <= packet_len && rcv_pkt_buf[pi] == 'D' && rcv_pkt_buf[pi+1] == 'I') {
                                         pi += 2; // skip DI
                                         while (pi < packet_len && rcv_pkt_buf[pi] != '~' && rcv_pkt_buf[pi] != 0) {
                                             senderID += (char)rcv_pkt_buf[pi];
                                             pi++;
                                         }
                                     } else {
                                         while (pi < packet_len && rcv_pkt_buf[pi] != '~') pi++;
                                        }
                                    } else {
                                        while (pi < packet_len && rcv_pkt_buf[pi] != '~') pi++;
                                    }
                                }
                            }
                            
                             sendSerialToApp(F("PROBE received from: "));
                             sendSerialToAppLn(senderID);
                             sendSerialToAppLn(F("PROBE synced — enabling hopping"));

                             // If the incoming packet is a PS (our own probe ACK), record bidirectional liveness
                             if (packet.type == "P2P_SYNC") {
                                 setPeerAcked();
                                 sendSerialToApp(F("P2P handshake from: "));
                                 sendSerialToAppLn(senderID);
                             }

                             // After syncing from this PRB, broadcast sync confirmation so peer also exits probe mode
                             if (wasInProbe && packet.sendDateTime.length() == 14) {
                                char confirmBuf[80];
                                snprintf(confirmBuf, sizeof(confirmBuf), "PR~DI%s~SD%s", bleGetDeviceIdShort(), packet.sendDateTime.c_str());
                                sendPacket((uint8_t*)confirmBuf, strlen(confirmBuf));
                                sendSerialToApp(F("Sent sync confirmation PRB to: "));
                                sendSerialToAppLn(senderID);

                                // Also send P2P_SYNC handshake so we know the peer heard us (bidirectional)
                                char ackBuf[60];
                                snprintf(ackBuf, sizeof(ackBuf), "PS~DI%s", bleGetDeviceIdShort());
                                sendPacket((uint8_t*)ackBuf, strlen(ackBuf));
                                
                                // Mark our own probe as acknowledged — we heard the peer and will hear us back
                                setPeerAcked();
                            }
                        } else if (!time_set) {
                            // Not a PRB but no GPS yet — accept sender's time as truth
                            if (packet.sendDateTime.length() == 14) {
                                time_set = true;
                                sendSerialToApp(F("RTC synced from peer ~SD: "));
                                sendSerialToAppLn(packet.sendDateTime);
                                adjustRTC(packet.sendDateTime);
                            } else {
                                autoSyncRTCFromPacket(packet);
                            }
                            
                            // If we just got time set, exit probe mode
                            if (time_set && inProbeMode) {
                                inProbeMode = false;
                                sendSerialToAppLn(F("Auto-synced via ~SD field — enabling hopping"));
                            }
                        }

                        // Always allow probe mode exit on any received packet
                        // This handles the case where both devices are in probe mode but only hear BEACONs (no PRBs)
                        if (inProbeMode) {
                            sendSerialToAppLn(F("Received valid packet — exiting probe mode"));
                            inProbeMode = false;

                            // During probe mode exit, always accept peer time as truth for sync.
                            // Gradual convergence only applies after initial sync is established.

                            // Handle P2P_SYNC handshake during probe exit
                            if (packet.type == "P2P_SYNC") {
                                setPeerAcked();
                                sendSerialToApp(F("P2P handshake from: "));
                                sendSerialToAppLn(packet.beacon_deviceId);
                            }

                            if (packet.sendDateTime.length() == 14) {
                                if (!time_set) {
                                    time_set = true;
                                    sendSerialToApp(F("RTC synced from peer ~SD: "));
                                    sendSerialToAppLn(packet.sendDateTime);
                                    adjustRTC(packet.sendDateTime);
                                } else {
                                    // Already have GPS — accept peer time anyway to resolve drift
                                    long localSecs = rtc.getDateTime().hour * 3600 + rtc.getDateTime().minute * 60 + rtc.getDateTime().second;
                                    long pktSecs = packet.sendDateTime.substring(8, 10).toInt() * 3600
                                                 + packet.sendDateTime.substring(10, 12).toInt() * 60
                                                 + packet.sendDateTime.substring(12, 14).toInt();
                                    long diff = abs(pktSecs - localSecs);
                                    
                                    if (diff > FrequencyHopSeconds) {
                                        // RTC is wildly wrong — accept peer time to get back in sync
                                        sendSerialToApp(F("RTC drifting by "));
                                        sendSerialToApp((String)diff);
                                        sendSerialToAppLn(F("s from peer — correcting"));
                                        adjustRTC(packet.sendDateTime);
                                    } else {
                                        // Within hop cycle tolerance — gradual nudge is fine
                                        autoSyncRTCFromPacket(packet);
                                    }
                                }
                            } else if (!time_set) {
                                time_set = true;
                                sendSerialToApp(F("RTC set from beacon"));
                                autoSyncRTCFromPacket(packet);
                            } else {
                                autoSyncRTCFromPacket(packet);
                            }
                        }

                        // Check for missing packets and process if nothing is missing
                        bool gapResolved = checkForMissingPackets(packet, rcv_pkt_buf, packet_len);
                        
                        // Track received counter for NAK resolution
                        markPacketReceived(packet.packetCounter);
                        
                        if (gapResolved) {
                            handlePacket(packet);
                            // Also process queued packets now that we have the next in sequence
                            processPacketQueue();
                            
                            // After processing a packet, check if it resolved any outstanding REQs
                            checkOutstandingReqs(packet.packetCounter);
                        }

                        // Reset sync loss timer on successful packet
                        syncLossTimer = millis();
                        lastPeerPacketTime = millis();
                        peerPacketReceived = true;

                    } else {
                        //No need to process
                    }
                } else if (state != RADIOLIB_ERR_RX_TIMEOUT) {
                    sendSerialToApp(F("Receive failed, code "));
                    sendSerialToAppLn((String)state);
                }
            }
            if (hopAfterTxRx) {
                hopAfterTxRx = false;
                setFrequency(hopToFrequency);  // Set the new frequency
            }
        }
    }

    // Discovery mode: stay on 869.47 MHz, listen for other devices
    if (deviceSettings.frequency_hopping_enabled && inProbeMode) {
        RTC_Date rtc_dt = rtc.getDateTime();
        unsigned long currentSecondsInDay = rtc_dt.hour * 3600 + rtc_dt.minute * 60 + rtc_dt.second;

        // Use the hop cycle number to trigger once per cycle
        unsigned long currentHopCycle = currentSecondsInDay / FrequencyHopSeconds;
        static unsigned long lastProbeTxCycle = 0;

        if (currentHopCycle > 0 && currentHopCycle != lastProbeTxCycle && !transmitFlag) {
            sendSerialToAppLn(F("PROBE tx — broadcasting on 869.47"));
            sendProbeBeacon();
            lastBeaconTime = millis();
            lastProbeTxCycle = currentHopCycle;

        } else if (currentHopCycle > 0) {
            // Second probe attempt: seconds 23-25 of each hop cycle (middle of interval)
            unsigned long secondsIntoHop = currentSecondsInDay % FrequencyHopSeconds;
            if (secondsIntoHop >= 23 && secondsIntoHop <= 25 && !transmitFlag) {
                static uint32_t lastMidProbe = 0;
                if (currentSecondsInDay != lastMidProbe) {
                    sendSerialToAppLn(F("PROBE tx (mid-cycle) — broadcasting on 869.47"));
                    sendProbeBeacon();
                    lastBeaconTime = millis();
                    lastMidProbe = currentSecondsInDay;
                }
            }
        }

        // If we heard a probe from another device during this window, exit probe mode
        if (heardProbeThisCycle && !inProbeMode) {
            // Already exited in PRB receive handler above
        }

        // Periodic status: show probe mode uptime every 3 hop cycles
        static unsigned long lastProbeLog = 0;
        if (currentSecondsInDay - lastProbeLog >= FrequencyHopSeconds * 3 && currentSecondsInDay > 0) {
            sendSerialToApp(F("PROBE mode — "));
            sendSerialToApp((String)currentSecondsInDay);
            sendSerialToAppLn(F("s since midnight"));
            lastProbeLog = currentSecondsInDay;
        }
    }

    // Post-sync probe broadcast: if we just exited probe mode, send one more PRB on 869.47
    // to announce our synced time to any peers still in probe mode with a different RTC offset.
    static uint32_t syncBroadcastAt = 0;
    if (syncBroadcastAt == 0 && !inProbeMode) {
        char confirmBuf[80];
        snprintf(confirmBuf, sizeof(confirmBuf), "PR~DI%s~SD%s", bleGetDeviceIdShort(), getFormattedDateTime().c_str());
        sendPacket((uint8_t*)confirmBuf, strlen(confirmBuf));
        syncBroadcastAt = millis() + 30000;  // Reset after 30s
    }
    if (deviceSettings.frequency_hopping_enabled && !inProbeMode) {
        RTC_Date currentTime = rtc.getDateTime();
        unsigned long sharedTime = currentTime.hour * 3600 + currentTime.minute * 60 + currentTime.second;
        unsigned long currentHopCycle = sharedTime / FrequencyHopSeconds;

        // Check if we should hop to discovery frequency (every 5 minutes, for one hop cycle)
        bool forceDiscoveryHop = ((sharedTime % DISCOVERY_PERIOD) < FrequencyHopSeconds);

        // During sync lock period after PRB receive, always stay on discovery freq
        if (syncLockUntilCycle > currentHopCycle) {
            forceDiscoveryHop = true;
            static unsigned long lastSyncLog = 0;
            RTC_Date now = rtc.getDateTime();
            unsigned long nowSecs = now.hour * 3600 + now.minute * 60 + now.second;
            if (nowSecs - lastSyncLog >= FrequencyHopSeconds) {
                sendSerialToAppLn(F("SYNC LOCK — staying on 869.47 for verification"));
                lastSyncLog = nowSecs;
            }
        }
        
        float newFrequency = currentFrequency;
        if (forceDiscoveryHop) {
            // Stay on discovery frequency for this hop cycle
            newFrequency = DISCOVERY_FREQUENCY;
        } else {
            newFrequency = getNextFrequency(sharedTime, sharedSeed);
        }

        if (newFrequency != currentFrequency) {
            sendSerialToApp(F("HOP → "));
            sendSerialToAppLn((String)(forceDiscoveryHop ? "869.47(resync)" : (String)newFrequency));

            if (!transmitFlag && operationDone) {
                hopToFrequency = newFrequency;
                hopAfterTxRx = true;
            } else {
                setFrequency(newFrequency);  // Set the new frequency
            }
            lastHopTime = sharedTime;  // Update the last hop time

            // If we're hopping away from discovery mode, reset heard probe flag
            if (newFrequency != DISCOVERY_FREQUENCY) {
                heardProbeThisCycle = false;
            }
        }

        // During a discovery hop, transmit a PRB beacon to announce ourselves
        unsigned long currentSecondsInDay = currentTime.hour * 3600 + currentTime.minute * 60 + currentTime.second;
        unsigned long secondsIntoHop = currentSecondsInDay % FrequencyHopSeconds;
        if (forceDiscoveryHop && secondsIntoHop >= PROBEBEACON_JITTER_MIN && secondsIntoHop < PROBEBEACON_JITTER_MAX
            && !transmitFlag && operationDone) {
            // Only send if we haven't already sent a PRB in this cycle
            if (!heardProbeThisCycle || (lastHopTime == 0)) {
                if (millis() - lastBeaconTime >= 5000) {
                    sendSerialToAppLn(F("→ TX probe on resync channel"));
                    sendProbeBeacon();
                    lastBeaconTime = millis();
                    heardProbeThisCycle = true;
                }
            }
        }

        // Log hop cycle summary every 3 cycles
        static unsigned long lastHopLog = 0;
        if (currentSecondsInDay - lastHopLog >= FrequencyHopSeconds * 3 && currentSecondsInDay > 0) {
            sendSerialToApp(F("CYC "));
            sendSerialToApp((String)(sharedTime / 60));
            sendSerialToAppLn(F(" min — freq:"));
            sendSerialToAppLn((String)currentFrequency);
            lastHopLog = currentSecondsInDay;
        }
    }

    // Periodically check outstanding REQs for retry / exhaustion
    if (nakInitialized && lastReceivedCounter > 0) {
        static unsigned long lastReqCheck = 0;
        if (millis() - lastReqCheck >= 1500) {
            checkOutstandingReqs(lastReceivedCounter);
            lastReqCheck = millis();
        }
    }

    // Send peer beacon periodically (only when not in probe mode)
    if (!inProbeMode) {
        unsigned long currentTime = millis();
        if (currentTime - lastBeaconTime >= PEER_BEACON_INTERVAL) {
            sendPeerBeacon();
            lastBeaconTime = currentTime;
        }
    }
}


int setFrequency(float freq) {
    // Avoid setting frequency if already set to the desired value
    if (currentFrequency == freq) {
        return RADIOLIB_ERR_NONE;  // Frequency already set
    }

    // Try setting the frequency
    int state = radio->setFrequency(freq);
    
    // If successful, update state and display
    if (state == RADIOLIB_ERR_NONE) {
        currentFrequency = freq;
        printFrequencyIcon(true);  // Show frequency on screen
        sendSerialToApp(F("Frequency set to: "));
    } else {
        // If unsuccessful, report error
        sendSerialToApp(F("Failed to set frequency, code: "));
    }

    // Log frequency and state
    sendSerialToApp((String)freq);
    sendSerialToApp(F(" - "));
    sendSerialToAppLn((String)state);
    
    // Reset transmit flag and start receiving
    transmitFlag = false;
    radio->startReceive();  // Always start receiving

    return state;
}


// Setup LoRa radio
bool setupLoRa() {
    // Guard: skip if we already successfully initialized this boot cycle.
    // Prevents "LoRa radio->begin(freq) OK" spam on every beacon/send/hop cycle.
    static bool loraAlreadyInitialized = false;
    if (loraAlreadyInitialized) {
        return true;
    }

    // Dispose old radio object before creating a new one (avoid heap leak)
    if (radio != nullptr) {
        delete radio;
        radio = nullptr;
    }

    hopAfterTxRx=false;
    transmitFlag = false;
    operationDone = false;

    // Initialize NAK reliability on first LoRa setup
    if (!nakInitialized) {
        initNakReliability();
    }

    pinMode(LoRa_Cs, OUTPUT);
    digitalWrite(LoRa_Cs, HIGH);
    pinMode(LoRa_Dio1, INPUT);
    pinMode(LoRa_Rst, OUTPUT);
    pinMode(LoRa_Busy, INPUT);
    delay(10);

    rfPort = new SPIClass(NRF_SPIM3, LoRa_Miso, LoRa_Sclk, LoRa_Mosi);
    rfPort->begin();

    digitalWrite(LoRa_Cs, HIGH);
    delay(1);

    SPISettings spiSettings;

    radio = new SX1262(new Module(LoRa_Cs, LoRa_Dio1, LoRa_Rst, LoRa_Busy, *rfPort, spiSettings));

    // Pass frequency to begin() — this sets TCXO voltage and DC-DC regulator internally,
    // which is critical for waking up the SX1262 on T-Echo. Matches reference Factory firmware pattern.
    int state = radio->begin(defaultFrequency);

    if (state != RADIOLIB_ERR_NONE) {
        sendSerialToAppLn("[BOOT] LoRa FAIL code=" + String(state));
        return false;
    } else {
        loraAlreadyInitialized = true;
        sendSerialToAppLn("[BOOT] LoRa OK");
    }

    radio->setDio1Action(setFlag);

    state = radio->setSpreadingFactor(deviceSettings.spreading_factor);
    delay(5);
    while (Serial.available()) Serial.read();
    state |= radio->setBandwidth(deviceSettings.bandwidth_idx / 1000.0);
    delay(5);
    while (Serial.available()) Serial.read();
    state |= radio->setCodingRate(deviceSettings.coding_rate_idx);

    if (state != RADIOLIB_ERR_NONE) {
        // some LoRa params failed
    }

    radio->setPreambleLength(24);
    radio->setOutputPower(20);
    radio->setCurrentLimit(120);

    bool lora_ready = radio->startReceive() == RADIOLIB_ERR_NONE;
    
    return lora_ready;
}

String getFormattedDateTime() {
    // Get the current date and time from the RTC
    RTC_Date dateTime = rtc.getDateTime();

    // Format it as YYYYMMDDHHMMSS
    char dateTimeStr[15];
    snprintf(dateTimeStr, sizeof(dateTimeStr), "%04d%02d%02d%02d%02d%02d",
             dateTime.year, dateTime.month, dateTime.day,
             dateTime.hour, dateTime.minute, dateTime.second);

    return String(dateTimeStr);
}

void storePacketInBuffer(uint8_t* pkt_buf, uint16_t len, unsigned int counter) {
    // Truncate to max size — no heap allocation needed
    uint16_t safeLen = len > MAX_PACKET_SIZE ? MAX_PACKET_SIZE : len;

    // Free previous slot data is automatic now (static buffer)
    
    // Store new packet in fixed-size buffer
    memcpy(retransmitPacketBuffer[bufferIndex].packetData, pkt_buf, safeLen);
    retransmitPacketBuffer[bufferIndex].packetLen = safeLen;
    retransmitPacketBuffer[bufferIndex].messageCounter = counter;
    retransmitPacketBuffer[bufferIndex].storedAt = millis();
    retransmitPacketBuffer[bufferIndex].valid = true;

    bufferIndex = (bufferIndex + 1) % RETRANSMIT_BUFFER_SIZE;  // Circular increment
}

void handleRetransmitRequest(unsigned int requestedCounter) {
    // Dedup: skip if we saw this REQ recently
    for (int i = 0; i < 8; i++) {
        if (recentReqs[i].counter == requestedCounter && 
            (millis() - recentReqs[i].timeReceived) < REQ_DEDUP_WINDOW_MS) {
            return;  // Already handling this one within dedup window
        }
    }

    // Add to dedup table (find empty slot first)
    int dedupSlot = -1;
    for (int i = 0; i < 8; i++) {
        if (!recentReqs[i].counter || (millis() - recentReqs[i].timeReceived) > REQ_DEDUP_WINDOW_MS) {
            dedupSlot = i;
            break;
        }
    }
    if (dedupSlot >= 0) {
        recentReqs[dedupSlot].counter = requestedCounter;
        recentReqs[dedupSlot].timeReceived = millis();
    }

    // Look for the packet in buffer — reject stale entries
    for (int i = 0; i < RETRANSMIT_BUFFER_SIZE; i++) {
        if (!retransmitPacketBuffer[i].valid) continue;
        
        unsigned long age = millis() - retransmitPacketBuffer[i].storedAt;
        if (age > MAX_BUFFER_AGE_MS) {
            retransmitPacketBuffer[i].valid = false;  // Discard stale entry
            continue;
        }
        
        if (retransmitPacketBuffer[i].messageCounter == requestedCounter) {
            sendSerialToApp(F("Resending packet with counter: "));
            sendSerialToAppLn((String)requestedCounter);

            char buf[50];
            snprintf(buf, sizeof(buf), "Resend: %d", requestedCounter);
            showError(buf);

            sendPacket(retransmitPacketBuffer[i].packetData, retransmitPacketBuffer[i].packetLen, requestedCounter);
            return;
        }
    }
    sendSerialToAppLn(F("Requested packet not found in buffer"));
    char buf[50];
    snprintf(buf, sizeof(buf), "Resend 404: %d", requestedCounter);
    showError(buf);
}

void sendPacket(uint8_t* pkt_buf, uint16_t len, unsigned int messageCounterOverride) {
    if (transmitFlag) {
        sendSerialToAppLn(F("Already in transmit, enqueueing packet"));
        enqueuePacket(pkt_buf, len);
        return;
    }



    // Use the provided message counter or generate a new one if no override is provided
    unsigned int currentMessageCounter = (messageCounterOverride > 0) ? messageCounterOverride : ++messageCounter;

    // The first 3 characters in pkt_buf are the packet type
    char typeBuffer[4];
    strncpy(typeBuffer, (char*)pkt_buf, 3);  // Extract the first 3 characters (packet type)
    typeBuffer[3] = '\0';

    // Estimate max header size: "~PC123456789~SD20230921183045~~" = ~30 chars
    char* send_pkt_buf;
    
    // Use stack buffer for small packets, heap only if payload exceeds max
    uint16_t headerLen = 0;
    char localBuf[MAX_PACKET_SIZE + 32];  // Header + packet data
    
    // Count type length to find where content starts (skip first 3 chars)
    char* contentStart = (char*)(pkt_buf + 3);
    uint16_t contentLen = len - 3;
    
    headerLen = snprintf(localBuf, sizeof(localBuf), "~PC%d~SD%s~~", currentMessageCounter, getFormattedDateTime().c_str());
    
    // Copy type prefix (first 3 chars)
    memmove(localBuf, pkt_buf, 3);
    
    // Append rest of original content after header length offset
    uint16_t newLen = headerLen + contentLen;
    
    if (newLen > MAX_PACKET_SIZE) {
        // Fall back to heap for oversized packets (shouldn't happen in normal use)
        send_pkt_buf = new char[newLen + 1];
        memmove(send_pkt_buf, pkt_buf, 3);
        snprintf(send_pkt_buf + 3, newLen - 2, "~PC%d~SD%s~~", currentMessageCounter, getFormattedDateTime().c_str());
        memcpy(send_pkt_buf + headerLen, contentStart, contentLen);
        send_pkt_buf[newLen] = '\0';
    } else {
        memmove(localBuf + 3, "~PC", 3);
        snprintf(localBuf + 3, sizeof(localBuf) - 3, "~PC%d~SD%s~~", currentMessageCounter, getFormattedDateTime().c_str());
        memcpy(localBuf + headerLen, contentStart, contentLen);
        localBuf[newLen] = '\0';
        send_pkt_buf = localBuf;
    }

    // Store the last message for retransmit after frequency hop
    static uint8_t lastMsgBuffer[MAX_PACKET_SIZE];
    uint16_t copyLen = (newLen > MAX_PACKET_SIZE) ? MAX_PACKET_SIZE : newLen;
    memcpy(lastMsgBuffer, send_pkt_buf, copyLen);
    lastMessageLength = copyLen;
    lastMessageCounter = currentMessageCounter;

    // Get time-on-air for logging
    timeOnAir = radio->getTimeOnAir(newLen);
    sendSerialToApp(F("Time-on-Air (us): "));
    sendSerialToAppLn((String)timeOnAir);

    sendSerialToApp((String)send_pkt_buf);
    sendSerialToApp("\n");

    //In case we need to resent, store it in the buffer
    storePacketInBuffer((uint8_t*)send_pkt_buf, newLen, currentMessageCounter);  // Store in buffer in case we need to resend

    // Start the transmission
    int state = radio->startTransmit((uint8_t*)send_pkt_buf, newLen);
    transmitFlag = true;

        if (state != RADIOLIB_ERR_NONE) {
            sendSerialToApp(F("Transmission start failed, code "));
            sendSerialToAppLn((String)state);
            char buf[50];
            snprintf(buf, sizeof(buf), "Lora Strt Trnsmt Err: %d", state);
            showError(buf);
            // Removed blocking setupLoRa() call here to avoid potential recursive 
            // delays or hangs in the critical radio path. Radio re-init should be handled 
            // asynchronously if needed, but for now we just report and stop transmission.
        }

    // Free heap fallback buffer if we used it
    if (newLen > MAX_PACKET_SIZE) {
        delete[] send_pkt_buf;
    }
}



// This function converts the string to uint8_t* and calls the main sendPacket
void sendPacket(const char* str) {
    uint16_t len = strlen(str);
    sendPacket((uint8_t*)str, len);  // Call the main function
}


// Function to calculate quality rating with 60-40 rule for SNR and RSSI
int calculateQuality(float rssi, float snr, bool ignoreSNR) {
    int rssiScore;
    int snrScore = 0;

    if (ignoreSNR) {
        rssiScore = map(rssi, -130, -20, 0, 100);
    } else {
        rssiScore = map(rssi, -130, -20, 0, 40);
        snrScore = map(snr, -20, 10, 0, 60);
    }

    return constrain(rssiScore + snrScore, 1, 100);
}

void sleepLoRa() {
    int state = radio->sleep();
    if (state == RADIOLIB_ERR_NONE) {
        sendSerialToAppLn(F("LoRa module is now in sleep mode."));
    } else {
        char buf[50];
        snprintf(buf, sizeof(buf), "LoRa Sleep Error: %d", state);
        showError(buf);
    }
}

// markFrequencyAsGood / markFrequencyAsBad removed — bad-channel tracking disabled (see comment at line 68).

// Check if the packet is duplicate by comparing packetCounter
bool isDuplicatePacket(Packet& packet) {
    if (packet.packetCounter == lastReceivedCounter) {
        return true;  // Duplicate packet
    }
    lastReceivedCounter = packet.packetCounter;  // Update the last received counter
    return false;
}

// Function to adjust RTC based on received timestamp
void adjustRTC(const String& dateTime) {
    if (dateTime.length() != 14) {
        return;  // Invalid timestamp, skip adjustment
    }

    // Extract components from the received timestamp (assumed format is "YYYYMMDDHHMMSS")
    int year = dateTime.substring(0, 4).toInt();
    int month = dateTime.substring(4, 6).toInt();
    int day = dateTime.substring(6, 8).toInt();
    int hour = dateTime.substring(8, 10).toInt();
    int minute = dateTime.substring(10, 12).toInt();
    int second = dateTime.substring(12, 14).toInt();

    // Adjust the RTC time
    rtc.setDateTime(year, month, day, hour, minute, second);
    //Serial.println(F("RTC adjusted to new time"));
}


// Function to check if the queue is full
bool isQueueFull() {
    return ((queueTail + 1) % SEND_PACKET_QUEUE_SIZE) == queueHead;
}

// Function to check if the queue is empty
bool isQueueEmpty() {
    return queueHead == queueTail;
}

// Function to enqueue a packet — fixed-size buffer, no heap allocation
void enqueuePacket(uint8_t* pkt_buf, uint16_t len) {
    if (isQueueFull()) {
        sendSerialToAppLn(F("Packet queue full, dropping packet"));
        return;
    }
    
    uint16_t safeLen = len > MAX_PACKET_SIZE ? MAX_PACKET_SIZE : len;
    memcpy(packetQueue[queueTail].packetData, pkt_buf, safeLen);
    packetQueue[queueTail].packetLen = safeLen;
    queueTail = (queueTail + 1) % SEND_PACKET_QUEUE_SIZE;
}


// Function to dequeue the next packet — copies into caller-provided buffer
bool dequeuePacket(uint8_t*& pkt_buf, uint16_t& len) {
    if (isQueueEmpty()) {
        return false;
    }

    // Get the packet data and length
    len = packetQueue[queueHead].packetLen;
    pkt_buf = packetQueue[queueHead].packetData;  // Return pointer to static buffer slot

    // Move to the next item in the queue
    queueHead = (queueHead + 1) % SEND_PACKET_QUEUE_SIZE;
    return true;
}


// Callback function to handle completion of transmission
void handleTransmissionComplete() {
    transmitFlag = false;

    uint8_t* nextPacketData = nullptr;
    uint16_t nextPacketLen = 0;

    // Check if there is a packet in the queue and transmit it
    if (dequeuePacket(nextPacketData, nextPacketLen)) {
        sendSerialToAppLn(F("Sending next packet from queue"));
        sendPacket(nextPacketData, nextPacketLen);

        // no heap free needed — buffer is static
    } else {
        // Packet queue is empty
        //Serial.println(F("Packet queue is empty"));
    }
}


// ============================================================
// NAK reliability helpers
// ============================================================

// Initialize the NAK reliability system — call once in setup()
void initNakReliability() {
    for (int i = 0; i < RETRANSMIT_BUFFER_SIZE; i++) {
        outstandingReqs[i].counter = 0;
        outstandingReqs[i].attempts = 0;
        outstandingReqs[i].resolved = false;
    }
    for (int i = 0; i < 8; i++) {
        recentReqs[i].counter = 0;
        recentReqs[i].timeReceived = 0;
    }
    lastReqSendTime = 0;
    nakInitialized = true;
    
    // Initialize all retransmit buffer slots as invalid
    for (int i = 0; i < RETRANSMIT_BUFFER_SIZE; i++) {
        retransmitPacketBuffer[i].valid = false;
        retransmitPacketBuffer[i].storedAt = 0;
    }
    
    sendSerialToAppLn(F("[NAK] Reliability system initialized"));
}

// Send a retransmission request with dedup and spacing logic
void sendRetransmitRequest(unsigned int counter) {
    if (!nakInitialized) return;
    
    // Dedup: check if we already have an outstanding REQ for this counter
    for (int i = 0; i < RETRANSMIT_BUFFER_SIZE; i++) {
        if (outstandingReqs[i].counter == counter && !outstandingReqs[i].resolved) {
            // Already requesting this — no need to resend
            return;
        }
    }
    
    // Check dedup window: skip if we recently saw this REQ at sender side
    for (int i = 0; i < 8; i++) {
        if (recentReqs[i].counter == counter && 
            (millis() - recentReqs[i].timeReceived) < REQ_DEDUP_WINDOW_MS) {
            return;
        }
    }
    
    // Enforce spacing between REQ transmissions
    unsigned long timeSinceLastReq = millis() - lastReqSendTime;
    if (timeSinceLastReq < REQ_PACKET_SPACING_MS && lastReqSendTime > 0) {
        delay(REQ_PACKET_SPACING_MS - timeSinceLastReq);
    }
    
    // Mark as outstanding
    int slot = -1;
    for (int i = 0; i < RETRANSMIT_BUFFER_SIZE; i++) {
        if (!outstandingReqs[i].counter || outstandingReqs[i].resolved) {
            slot = i;
            break;
        }
    }
    if (slot < 0) return;  // No free slot
    
    outstandingReqs[slot].counter = counter;
    outstandingReqs[slot].attempts = 1;
    outstandingReqs[slot].resolved = false;
    outstandingReqs[slot].lastSentTime = millis();
    
    // Send the REQ packet
    char requestBuf[50];
    snprintf(requestBuf, sizeof(requestBuf), "REQ%d", counter);
    sendPacket((uint8_t*)requestBuf, strlen(requestBuf));
    lastReqSendTime = millis();
    
    sendSerialToApp(F("[NAK] REQ sent for counter: "));
    sendSerialToAppLn((String)counter);
}

// Check if a specific counter has been received (resolves outstanding REQs)
bool markPacketReceived(unsigned int counter) {
    // Check if any outstanding REQ matches this counter
    for (int i = 0; i < RETRANSMIT_BUFFER_SIZE; i++) {
        if (outstandingReqs[i].counter == counter && !outstandingReqs[i].resolved) {
            outstandingReqs[i].resolved = true;
            sendSerialToApp(F("[NAK] REQ for counter "));
            sendSerialToApp((String)counter);
            sendSerialToAppLn(F(" resolved — packet received"));
            return true;
        }
    }
    return false;  // No matching outstanding REQ
}

// Check outstanding REQs: retry expired ones, discard exhausted ones
void checkOutstandingReqs(unsigned int processedCounter) {
    if (!nakInitialized) return;
    
    unsigned long now = millis();
    
    for (int i = 0; i < RETRANSMIT_BUFFER_SIZE; i++) {
        // Skip resolved or empty slots
        if (!outstandingReqs[i].counter || outstandingReqs[i].resolved) continue;
        
        // Check if the retransmitted packet arrived
        if (outstandingReqs[i].counter == processedCounter) {
            outstandingReqs[i].resolved = true;
            sendSerialToApp(F("[NAK] REQ for counter "));
            sendSerialToApp((String)processedCounter);
            sendSerialToAppLn(F(" resolved via processed counter"));
            continue;
        }
        
        // Check timeout and retry
        unsigned long elapsed = now - outstandingReqs[i].lastSentTime;
        unsigned long retryDelay = (unsigned long)(REQ_BASE_TIMEOUT_MS << (outstandingReqs[i].attempts - 1));
        
        if (elapsed >= retryDelay) {
            outstandingReqs[i].attempts++;
            
            if (outstandingReqs[i].attempts > REQ_RETRY_MAX) {
                // Give up — gap is permanent, advance past it
                sendSerialToApp(F("[NAK] Giving up on counter "));
                sendSerialToApp((String)outstandingReqs[i].counter);
                sendSerialToAppLn(F(" after max retries"));
                
                // Advance lastReceivedCounter to skip this gap permanently
                if (outstandingReqs[i].counter > lastReceivedCounter) {
                    lastReceivedCounter = outstandingReqs[i].counter;
                }
                
                outstandingReqs[i].resolved = true;
                outstandingReqs[i].counter = 0;
            } else {
                // Retry with exponential backoff
                unsigned long nextDelay = (unsigned long)(REQ_BASE_TIMEOUT_MS << (outstandingReqs[i].attempts - 1));
                sendSerialToApp(F("[NAK] Retrying REQ for counter "));
                sendSerialToApp((String)outstandingReqs[i].counter);
                sendSerialToApp(F(" (attempt "));
                sendSerialToApp((String)outstandingReqs[i].attempts);
                sendSerialToApp(F(", delay "));
                sendSerialToApp((String)nextDelay);
                sendSerialToAppLn(F("ms)"));
                
                outstandingReqs[i].lastSentTime = now;
                
                // Send the REQ again
                char requestBuf[50];
                snprintf(requestBuf, sizeof(requestBuf), "REQ%d", outstandingReqs[i].counter);
                sendPacket((uint8_t*)requestBuf, strlen(requestBuf));
            }
        }
    }
}

