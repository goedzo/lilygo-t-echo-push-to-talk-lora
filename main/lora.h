#ifndef LORA_H
#define LORA_H

#include <RadioLib.h>
#include <stdint.h>

#define QUALITY_THRESHOLD 50  // Define a threshold for channel quality (value can be adjusted)

// Define the FrequencyStatus struct before using it
struct FrequencyStatus {
    float frequency;
    bool isGood;
};

// Global variables
extern SX1262* radio;      // Pointer to the SX1262 radio module
extern SPIClass* rfPort;   // SPI port for the radio module

// Flags for radio operation
extern volatile bool operationDone;  // Flag to indicate a completed radio operation (sent/received)
extern bool transmitFlag;            // Flag to indicate whether we are transmitting a packet
extern size_t timeOnAir;             // Variable to store time-on-air for transmitted packets

// Frequency hopping related
extern float defaultFrequency;
extern float currentFrequency;       // Variable to track the current operating frequency
extern float startFreq;              // Start frequency for hopping
extern float endFreq;                // End frequency for hopping
extern float stepSize;               // Step size for frequency hopping
extern int numFrequencies;           // Number of frequencies calculated at runtime
extern FrequencyStatus* frequencyMap;  // Pointer to store frequency statuses (good/bad)

// Function prototypes
void setFlag(void);                              // Callback function for radio interrupts (transmit/receive complete)
void checkLoraPacketComplete(void);              // Function to check if a packet transmission or reception is complete
bool setupLoRa(void);                            // Function to initialize the LoRa module
void sendPacket(uint8_t* pkt_buf, uint16_t len); // Function to send a data packet (non-blocking)
void sendPacket(const char* str);                // Function to send a string packet (non-blocking)
void sleepLoRa(void);                            // Function to put the LoRa module into sleep mode
void initializeFrequencyMap(void);               // Function to initialize the frequency map for hopping
int calculateQuality(float rssi, float snr, bool ignoreSNR); // Function to calculate channel quality score

// Helper functions for frequency status management
void markFrequencyAsGood(float freq);            // Mark a frequency as "good" based on quality score
void markFrequencyAsBad(float freq);             // Mark a frequency as "bad" based on quality score
int setFrequency(float freq);

#endif // LORA_H
