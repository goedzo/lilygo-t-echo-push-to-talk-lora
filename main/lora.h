#ifndef LORA_H
#define LORA_H

#include <RadioLib.h>
#include <stdint.h>

#include "packet.h"

#define QUALITY_THRESHOLD 50  // Define a threshold for channel quality (value can be adjusted)
#define RETRANSMIT_BUFFER_SIZE 100  // Buffer size for packet retransmission — effectively infinite for realistic loss (~500s at 5s sends)
#define RECEIVE_PACKET_QUEUE_SIZE 5  // Buffer size for storing newer packets
#define MAX_PACKET_SIZE 128        // Maximum packet payload in bytes (fits LoRa max, eliminates heap allocation)

// NAK reliability parameters
#define REQ_RETRY_MAX 4                       // Max attempts per outstanding REQ
#define REQ_BASE_TIMEOUT_MS 4000              // Initial REQ timeout: 4s (longer than SF data airtime)
#define REQ_BACKOFF_MULT 2                    // Exponential backoff multiplier for REQ retries
#define REQ_PACKET_SPACING_MS 2500            // Minimum gap between consecutive outgoing REQ packets
#define REQ_DEDUP_WINDOW_MS 8000              // Ignore duplicate REQs within this window at sender
#define MAX_BUFFER_AGE_MS 120000              // Discard buffer entries older than 2 minutes

// Enum for Frequency Status
enum FrequencyStatusEnum {
    FREQUENCY_GOOD = 0,
    FREQUENCY_BAD_LOCAL = 1,    // Marked bad by the local device
    FREQUENCY_BAD_EXTERNAL = 2, // Marked bad by an external device
    FREQUENCY_BAD_BOTH = 3      // Marked bad by both local and external devices
};

// Structure to track frequency status
struct FrequencyStatus {
    float frequency;
    FrequencyStatusEnum status;  // Status of the frequency (good, bad by local, bad by external, or both)
};

// Fixed-size packet buffer for retransmission — no heap allocation needed
struct PacketBuffer {
    uint8_t  packetData[MAX_PACKET_SIZE];  // Pre-allocated fixed buffer
    uint16_t packetLen;
    unsigned int messageCounter;
    uint32_t storedAt;          // millis() timestamp when this slot was written
    bool     valid;             // True if slot contains a usable entry
};

// Track an outstanding REQ waiting for the retransmitted packet to arrive
struct PendingReq {
    unsigned int counter;       // The missing counter we're requesting
    uint32_t lastSentTime;      // millis() when this REQ was last sent
    uint8_t  attempts;          // How many times we've sent this REQ so far
    bool     resolved;          // True once the retransmitted packet has been received
};

// Track incoming REQs for dedup at sender side — prevents double-retransmit
struct ReqDedupEntry {
    unsigned int counter;       // The requested counter value
    uint32_t timeReceived;      // millis() when we last saw this REQ
};

// Structure for queueing newer packets (received out of order)
struct PacketQueue {
    uint8_t  packetData[MAX_PACKET_SIZE];
    uint16_t packetLen;
    unsigned int packetCounter;
};

// Structure for packet queue
struct PacketQueueEntry {
    uint8_t  packetData[MAX_PACKET_SIZE];
    uint16_t packetLen;
};



// External declarations of important LoRa-related variables
extern SX1262* radio;
extern float defaultFrequency;
extern float currentFrequency;
extern FrequencyStatus* frequencyMap;  // Pointer to store frequency statuses (good/bad)
extern float startFreq;
extern float endFreq;
extern float stepSize;
extern int numFrequencies;

// Flags for radio operation
extern volatile bool operationDone;  // Flag to indicate a completed radio operation (sent/received)
extern bool transmitFlag;            // Flag to indicate whether we are transmitting a packet
extern size_t timeOnAir;             // Variable to store time-on-air for transmitted packets
extern bool hopAfterTxRx;            // Hop when our last action was completed
extern float hopToFrequency;

// Function declarations
void initializeFrequencyMap();
float getNextFrequency(unsigned long sharedTime, unsigned long sharedSeed);
void checkLoraPacketComplete();
bool setupLoRa();
void sendPacket(uint8_t* pkt_buf, uint16_t len, unsigned int messageCounterOverride = 0);
void sendPacket(const char* str);
void sleepLoRa();
void markFrequencyAsGood(float freq);
void markFrequencyAsBad(float freq, bool local);
int setFrequency(float freq);
void shareFrequencyMap();  // Function to share the frequency map
void updateFrequencyMap(const unsigned char* receivedData, int len);  // Function to update the frequency map based on received data
void handleMapSharing();  // Function to handle map sharing logic
unsigned char calculateChecksum(const unsigned char* data, int len);  // Function to calculate checksum for data validation
int calculateQuality(float rssi, float snr, bool ignoreSNR);
String getFormattedDateTime();

// Packet handling related functions
void storePacketInBuffer(uint8_t* pkt_buf, uint16_t len, unsigned int counter);  // Store packet in buffer for retransmission
void handleRetransmitRequest(unsigned int requestedCounter);  // Handle packet retransmission request
void storePacketInQueue(uint8_t* pkt_buf, uint16_t len, unsigned int counter);  // Store newer packets in queue
void processPacketQueue();  // Process the queued packets
void handleRetransmitRequestComplete();  // Handle retransmission completion and process queued packets
bool checkForMissingPackets(Packet& packet);  // Check for missing packets and request retransmission if necessary
void adjustRTC(const String& dateTime);
void handleTransmissionComplete();
void enqueuePacket(uint8_t* pkt_buf, uint16_t len);

// NAK reliability helpers
void initNakReliability();                     // Called once in setup()
void checkOutstandingReqs(unsigned int processedCounter);  // Check if any resolved REQs, retry expired ones
bool markPacketReceived(unsigned int counter);           // Track that a specific counter arrived (resolves outstanding REQs)
void sendRetransmitRequest(unsigned int counter);        // Send REQ with dedup + spacing logic
// Peer beacon & liveness tracking
#define PEER_BEACON_INTERVAL 47000  // 47s, matches hop cycle
#define PEER_TIMEOUT 94000          // 94s = 2 beacon cycles

extern unsigned long lastPeerPacketTime;
bool isPeerAlive();
void sendPeerBeacon();
const char* bleGetDeviceIdShort();

// Peer roster (BEACON mode)
#define MAX_ROSTER_PEERS 8
struct PeerEntry {
    String  deviceId;     // Last 8 hex chars of MAC
    double  lat;          // From ~GP field
    double  lon;          // From ~GP field
    uint8_t battery;      // From ~BT field
    unsigned long lastSeen; // millis() timestamp
    float   distanceM;    // Computed from GPS location
};

extern PeerEntry peerRoster[MAX_ROSTER_PEERS];
extern int     peerRosterCount;
extern bool    inBeaconMode;
void beaconAddOrUpdate(const Packet& packet);
void beaconDisplayRoster(uint8_t line);

// Probe-based frequency hopping discovery
#define PROBE_FREQUENCY       startFreq   // Fixed known channel: 863.0 MHz
#define PROBE_INTERVAL_HOPS   4           // Send probe every 4 hops (188s)
extern bool inProbeMode;                 // True while waiting for time sync via probes
extern unsigned long firstBootMillis;    // Boot timestamp in millis for probe logic

#endif // LORA_H
