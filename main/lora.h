#ifndef LORA_H
#define LORA_H

#include <RadioLib.h>
#include <stdint.h>

#include "packet.h"

#define QUALITY_THRESHOLD 50  // Define a threshold for channel quality (value can be adjusted)
#define PACKET_BUFFER_SIZE 5  // Buffer size for packet retransmission
#define NEWER_PACKET_QUEUE_SIZE 5  // Buffer size for storing newer packets

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

// Structure for packet buffer (used for retransmission)
struct PacketBuffer {
    uint8_t* packetData;
    uint16_t packetLen;
    unsigned int messageCounter;
};

// Structure for queueing newer packets (received out of order)
struct PacketQueue {
    uint8_t* packetData;
    uint16_t packetLen;
    unsigned int packetCounter;
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
void sendPacket(uint8_t* pkt_buf, uint16_t len);
void sendPacket(const char* str);
void sleepLoRa();
void markFrequencyAsGood(float freq);
void markFrequencyAsBad(float freq, bool local);
int setFrequency(float freq);
void shareFrequencyMap();  // Function to share the frequency map
void updateFrequencyMap(const unsigned char* receivedData, int len);  // Function to update the frequency map based on received data
void handleMapSharing();  // Function to handle map sharing logic
unsigned char calculateChecksum(const unsigned char* data, int len);  // Function to calculate checksum for data validation
bool isSyncLost();  // Function to detect loss of synchronization
int calculateQuality(float rssi, float snr, bool ignoreSNR);
String getFormattedDateTime();

// Packet handling related functions
void storePacketInBuffer(uint8_t* pkt_buf, uint16_t len, unsigned int counter);  // Store packet in buffer for retransmission
void handleRetransmitRequest(unsigned int requestedCounter);  // Handle packet retransmission request
void storePacketInQueue(uint8_t* pkt_buf, uint16_t len, unsigned int counter);  // Store newer packets in queue
void processPacketQueue();  // Process the queued packets
void handleRetransmitRequestComplete();  // Handle retransmission completion and process queued packets
bool checkForMissingPackets(Packet& packet);  // Check for missing packets and request retransmission if necessary

#endif // LORA_H
