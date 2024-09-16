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
#include <time.h>  // For RTC time management
#include <stdlib.h>  // For random number generation

SX1262* radio = nullptr;
SPIClass* rfPort = nullptr;

float defaultFrequency = 869.47;  // Default frequency
float currentFrequency = defaultFrequency;

// Frequency hopping related variables
float startFreq = 863.0;
float endFreq = 869.65;
float stepSize = 0.01;
int numFrequencies = (endFreq - startFreq) / stepSize;
FrequencyStatus* frequencyMap = nullptr;
int FrequencyHopSeconds = 17; //After how many seconds do we hop to the next frequency?

volatile bool operationDone = false;  // Flag to indicate radio operation is done
bool transmitFlag = false;            // Flag for transmission state
size_t timeOnAir = 0;                 // Time-on-air for transmitted packets

bool hopAfterTxRx = false;            // Hop when our last action was completed
float hopToFrequency;

unsigned long lastHopTime = 0;  // Time of last hop

unsigned long syncLossTimer = 0;  // Timer for detecting lost synchronization

bool mapChanged = false;  // Flag to track if the map has changed locally
unsigned long lastMapShareTime = 0;  // Last time the map was shared
unsigned long mapShareDelay = 0;  // Random delay for map sharing

// Global message counter to find out if we have missed messages
unsigned int messageCounter = 0;

// Function to initialize frequency map
void initializeFrequencyMap() {
    SerialMon.print("initializeFrequencyMap Initializing ...  ");
    frequencyMap = new FrequencyStatus[numFrequencies];
    for (int i = 0; i < numFrequencies; i++) {
        frequencyMap[i].frequency = startFreq + (i * stepSize);
        frequencyMap[i].status = FREQUENCY_GOOD;  // Start by assuming all frequencies are good
    }
}

// Pseudo-random frequency hopping based on a shared time source and randomness
float getNextFrequency(unsigned long sharedTime, unsigned long sharedSeed) {
    // Combine sharedTime and sharedSeed for randomness
    unsigned long randomValue = sharedTime ^ sharedSeed;  // XOR the time with the seed for randomness
    Serial.print(F("SharedTime: ")); 
    Serial.println(sharedTime);
    Serial.print(F("SharedSeed: "));
    Serial.println(sharedSeed);
    Serial.print(F("RandomValue: "));
    Serial.println(randomValue);

    // Generate a random starting index
    int startingIndex = randomValue % numFrequencies;
    Serial.print(F("Starting Index: "));
    Serial.println(startingIndex);

    // Loop through the frequencies starting from the random index
    for (int i = 0; i < numFrequencies; i++) {
        int index = (startingIndex + i) % numFrequencies;  // Wrap around if we exceed the range
        Serial.print(F("Checking Frequency Index: "));
        Serial.println(index);
        Serial.print(F("Frequency: "));
        Serial.println(frequencyMap[index].frequency);
        Serial.print(F("Frequency Status: "));
        Serial.println(frequencyMap[index].status);

        if (frequencyMap[index].status == FREQUENCY_GOOD) {
            Serial.print(F("Selected Frequency: "));
            Serial.println(frequencyMap[index].frequency);
            return frequencyMap[index].frequency;  // Return the good frequency
        }
    }

    // If no good frequency is found, fallback to startFreq
    Serial.println(F("No good frequency found, falling back to startFreq"));
    return defaultFrequency;
}


// Function to detect loss of synchronization
bool isSyncLost() {
    RTC_Date currentTime = rtc.getDateTime();  // Get the current time from RTC
    unsigned long currentSharedTime = currentTime.hour * 3600 + currentTime.minute * 60 + currentTime.second;

    // Calculate expected hop time based on last known good map sharing time
    unsigned long expectedSharedTime = lastMapShareTime + (millis() - syncLossTimer) / 1000;  // Convert to seconds
    
    // Calculate the time difference between current and expected
    long timeDifference = abs((long)(currentSharedTime - expectedSharedTime));

    // Consider sync lost if the time difference exceeds a threshold (e.g., 5 seconds)
    if (timeDifference > 5) {  // Adjustable threshold
        Serial.print(F("Sync loss detected. Time difference: "));
        Serial.println(timeDifference);
        return true;
    }

    // If the time difference is within the threshold, sync is not lost
    return false;
}


// Simple checksum calculation for validating received maps
unsigned char calculateChecksum(const unsigned char* data, int len) {
    unsigned char checksum = 0;
    for (int i = 0; i < len; i++) {
        checksum ^= data[i];  // XOR all bytes
    }
    return checksum;
}

// Function to share the frequency map with other devices
void shareFrequencyMap() {
    char send_pkt_buf[155];
    snprintf(send_pkt_buf, sizeof(send_pkt_buf), "MAP");  // Add "MAP" header

    unsigned char frequencyStatusData[numFrequencies];
    for (int i = 0; i < numFrequencies; i++) {
        frequencyStatusData[i] = (frequencyMap[i].status == FREQUENCY_BAD_LOCAL) ? 0 : 1;  // Only share locally marked bad frequencies
    }

    // Append the frequency status data to the packet
    memcpy(send_pkt_buf + 3, frequencyStatusData, numFrequencies);

    // Calculate checksum and append it
    unsigned char checksum = calculateChecksum((unsigned char*)send_pkt_buf, numFrequencies + 3);
    send_pkt_buf[numFrequencies + 3] = checksum;

    // Send the frequency map packet with the "MAP" header
    sendPacket((uint8_t*)send_pkt_buf, numFrequencies + 4);  // +4 to include header and checksum

    // Reset the mapChanged flag and set the last shared time
    mapChanged = false;
    lastMapShareTime = millis();
}

// Function to update local frequency map based on received data
void updateFrequencyMap(const unsigned char* receivedData, int len) {
    // Validate the checksum
    if (calculateChecksum(receivedData, len - 1) != receivedData[len - 1]) {
        Serial.println("Invalid frequency map received");
        return;  // Invalid map, do not update
    }

    // Merge the received map into the local map, keeping locally marked bad channels
    for (int i = 0; i < len - 1 && i < numFrequencies; i++) {
        if (frequencyMap[i].status == FREQUENCY_GOOD && receivedData[i] == 0) {
            // If it was good locally but bad in the received map, mark it bad externally
            frequencyMap[i].status = FREQUENCY_BAD_EXTERNAL;
        } else if (frequencyMap[i].status == FREQUENCY_BAD_LOCAL && receivedData[i] == 0) {
            // If it was bad locally and bad in the received map, mark it bad by both
            frequencyMap[i].status = FREQUENCY_BAD_BOTH;
        }
    }
}

void setFlag(void) {
    operationDone = true;
}

// Function to handle map sharing logic
void handleMapSharing() {
    if(deviceSettings.frequency_hopping_enabled) {
      if (mapChanged) {
          // Check if it's time to share the map
          unsigned long currentTime = millis();
          if (currentTime - lastMapShareTime >= mapShareDelay) {
              shareFrequencyMap();
          }
      }
    }
}

// Handle AFH and packet completion
void checkLoraPacketComplete() {
    if (operationDone) {
        operationDone = false;

        if (transmitFlag) {
            transmitFlag = false;
            int state = radio->finishTransmit();
            if (state == RADIOLIB_ERR_NONE) {
                Serial.println("Packet was Sent, finishTransmit");
            } else {
                Serial.print(F("Sent failed, code "));
                Serial.println(state);
            }
            if(hopAfterTxRx) {
                hopAfterTxRx=false;
                setFrequency(hopToFrequency);  // Set the new frequency
            }
            else {
                radio->startReceive();  // Start receiving after transmission

            }
        } else {
            uint16_t packet_len = radio->getPacketLength(false);
            uint16_t irqStatus = radio->getIrqStatus();
            unsigned char rcv_pkt_buf[MAX_PKT];

            if (irqStatus & RADIOLIB_SX126X_IRQ_RX_DONE) {
                int state = radio->readData(rcv_pkt_buf, packet_len);
                if (state == RADIOLIB_ERR_NONE) {
                    rcv_pkt_buf[packet_len] = '\0';  // Null-terminate the received packet

                    Packet packet;
                    if (packet.parsePacket(rcv_pkt_buf, packet_len)) {
                        handlePacket(packet);

                        // Update quality of the current frequency
                        float rssi = radio->getRSSI();
                        float snr = radio->getSNR();
                        int qualityScore = calculateQuality(rssi, snr, false);

                        if (qualityScore >= QUALITY_THRESHOLD) {
                            markFrequencyAsGood(currentFrequency);
                        } else {
                            markFrequencyAsBad(currentFrequency, true);  // Mark bad by local
                            mapChanged = true;  // Flag that a local change occurred
                            mapShareDelay = random(1000, 5000);  // Randomize the map share delay (1-5 seconds)
                        }

                        // Update local frequency map if frequency map packet is received
                        if (packet.type == "MAP") {
                            updateFrequencyMap(rcv_pkt_buf + 3, packet_len - 4);  // Skip the header and checksum
                        }

                        // Reset sync loss timer on successful packet
                        syncLossTimer = millis();
                    } else {
                        Serial.println("Error packet received");
                    }
                } else if (state != RADIOLIB_ERR_RX_TIMEOUT) {
                    Serial.print(F("Receive failed, code "));
                    Serial.println(state);
                }
            }
            if(hopAfterTxRx){
                hopAfterTxRx=false;
                setFrequency(hopToFrequency);  // Set the new frequency
            }
            else {
                radio->startReceive();  // Prepare to receive the next packet
            }
        }
    }

    // Handle frequency hopping using RTC time (hours, minutes, and seconds)
    if (deviceSettings.frequency_hopping_enabled && time_set) {
        RTC_Date currentTime = rtc.getDateTime();  // Get current time from the RTC
        int currentSeconds = currentTime.second;   // Get current seconds value

        // Only hop if no transmission/reception is occurring
        if (currentSeconds % FrequencyHopSeconds == 0 && currentSeconds != lastHopTime) {  // Adjust the hop interval if needed

            Serial.print(F("Frequency hop at "));
            Serial.println(currentSeconds);

            // Create a shared time using hours, minutes, and seconds
            unsigned long sharedTime = currentTime.hour * 3600 + currentTime.minute * 60 + currentTime.second;

            // Use shared time to get the next frequency using the frequency hopping algorithm
            float newFrequency = getNextFrequency(sharedTime, sharedSeed);

            // Check for lost synchronization
            if (isSyncLost()) {
                Serial.println(F("Sync lost, reinitializing synchronization..."));
                // Reinitialize sync using the current RTC time
                lastMapShareTime = sharedTime;
                syncLossTimer = millis();  // Reset the sync loss timer

                lastHopTime = currentSeconds;  // Update the last hop time

                // Use the reinitialized shared time to get the next frequency
                newFrequency = getNextFrequency(sharedTime, sharedSeed);
            }
            if(!transmitFlag && operationDone) {
                //We must delay until our next radio action is completed
                hopToFrequency=newFrequency;
                hopAfterTxRx=true;

            }
            else {
                //We can hop, as nothing is happening on the radio
                setFrequency(newFrequency);  // Set the new frequency
            }
            
            lastHopTime = currentSeconds;  // Update the last hop time
        }
    }

    // Handle the map sharing logic
    handleMapSharing();
}

// Function to set the frequency and update global state
int setFrequency(float freq) {
    int state = radio->setFrequency(freq);
    if (state == RADIOLIB_ERR_NONE) {
        currentFrequency = freq;
        printFrequencyIcon(true);  // Show frequency on screen
        Serial.print(F("Setting frequency "));
        Serial.println(freq);
    } else {
        Serial.print(F("Failed to set frequency, code "));
        Serial.print(freq);
        Serial.print(F(" - "));
        Serial.println(state);
    }
    transmitFlag = false;
    //Listen to this frequency
    radio->startReceive();
    return state;
}

// Setup LoRa radio
bool setupLoRa() {
    Serial.print(F("Initializing Lora ... "));

    hopAfterTxRx=false;

    transmitFlag = false;
    operationDone = false;

    rfPort = new SPIClass(NRF_SPIM3, LoRa_Miso, LoRa_Sclk, LoRa_Mosi);
    rfPort->begin();
    SPISettings spiSettings;

    radio = new SX1262(new Module(LoRa_Cs, LoRa_Dio1, LoRa_Rst, LoRa_Busy, *rfPort, spiSettings));

    int state = radio->begin(defaultFrequency);  // Use currentFrequency
        if (state == RADIOLIB_ERR_NONE) {
            Serial.println(F("success!"));
        } else {
            Serial.print(F("failed, code "));
            Serial.println(state);
    }

    radio->setDio1Action(setFlag);

    state = radio->setSpreadingFactor(deviceSettings.spreading_factor);
    state |= radio->setBandwidth(deviceSettings.bandwidth_idx / 1000.0);
    state |= radio->setCodingRate(deviceSettings.coding_rate_idx);

    if (state != RADIOLIB_ERR_NONE) {
        Serial.print(F("Failed to set LoRa parameters, code "));
        Serial.println(state);
    }

    radio->setPreambleLength(24);
    radio->setOutputPower(20);
    radio->setCurrentLimit(120);
/*
    state = setFrequency(defaultFrequency);
    if (state == RADIOLIB_ERR_NONE) {
        Serial.println(F("Setup lora success!"));
    } else {
        Serial.print(F("failed, code "));
        Serial.println(state);
    }
*/

    Serial.println(F("LoRa setup completed successfully!"));
    return radio->startReceive() == RADIOLIB_ERR_NONE;
}

void sendPacket(uint8_t* pkt_buf, uint16_t len) {
    if (transmitFlag) {
        showError("Already in transmit, skipping");
        transmitFlag = false;
        return;
    }

    // Increment the message counter for each new packet
    messageCounter++;


    // Calculate the size of the new packet buffer (original packet size + 4 digits for message counter)
    uint16_t newLen = len + 4;  // Original length + 4 digits for the message counter
    // Create a dynamically sized buffer for the packet
    char* send_pkt_buf = new char[newLen + 1];  // +1 for null terminator
    // Copy the original packet data to the new buffer
    memcpy(send_pkt_buf, pkt_buf, len);
    // Append the message counter to the end of the packet (as a 4-digit number)
    snprintf(send_pkt_buf + len, newLen + 1 - len, "%04u", messageCounter);

    timeOnAir = radio->getTimeOnAir(len);
    Serial.print(F("Time-on-Air (ms): "));
    Serial.println(timeOnAir);

    int state = radio->startTransmit(pkt_buf, len);
    transmitFlag = true;

    if (state != RADIOLIB_ERR_NONE) {
        Serial.print(F("Transmission start failed, code "));
        Serial.println(state);
        char buf[50];
        snprintf(buf, sizeof(buf), "Lora Strt Trnsmt Err: %d", state);
        showError(buf);
        setupLoRa();  // Reinitialize the radio
    }

    // Free the dynamically allocated buffer
    delete[] send_pkt_buf;

}

void sendPacket(const char* str) {
    if (transmitFlag) {
        showError("Already in transmit, skipping");
        return;
    }

    // Increment the message counter
    messageCounter++;

    // Calculate the size of the new packet buffer (original packet size + 4 digits for the message counter)
    uint16_t len = strlen(str);
    uint16_t newLen = len + 4;  // Original length + 4 digits for the message counter

    // Create a dynamically sized buffer for the packet
    char* send_pkt_buf = new char[newLen + 1];  // +1 for null terminator

    // Copy the original string data to the new buffer
    strcpy(send_pkt_buf, str);

    // Append the message counter to the end of the packet (as a 4-digit number)
    snprintf(send_pkt_buf + len, newLen + 1 - len, "%04u", messageCounter);

    // Send the modified packet
    timeOnAir = radio->getTimeOnAir(newLen);
    Serial.print(F("Time-on-Air (ms): "));
    Serial.println(timeOnAir);

    int state = radio->startTransmit((uint8_t*)send_pkt_buf, newLen);
    transmitFlag = true;

    // Clean up the dynamically allocated buffer
    delete[] send_pkt_buf;

    if (state != RADIOLIB_ERR_NONE) {
        Serial.print(F("Transmission start failed, code "));
        Serial.println(state);
        char buf[50];
        snprintf(buf, sizeof(buf), "Lora Strt Trnsmt Err: %d", state);
        showError(buf);
        setupLoRa();  // Reinitialize the radio
    }
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
        Serial.println(F("LoRa module is now in sleep mode."));
    } else {
        char buf[50];
        snprintf(buf, sizeof(buf), "LoRa Sleep Error: %d", state);
        showError(buf);
    }
}

// Mark frequency as good
void markFrequencyAsGood(float freq) {
    for (int i = 0; i < numFrequencies; i++) {
        if (frequencyMap[i].frequency == freq) {
            frequencyMap[i].status = FREQUENCY_GOOD;
            break;
        }
    }
}

// Mark frequency as bad (local or external)
void markFrequencyAsBad(float freq, bool local) {
    for (int i = 0; i < numFrequencies; i++) {
        if (frequencyMap[i].frequency == freq) {
            if (local) {
                if (frequencyMap[i].status == FREQUENCY_BAD_EXTERNAL) {
                    frequencyMap[i].status = FREQUENCY_BAD_BOTH;
                } else {
                    frequencyMap[i].status = FREQUENCY_BAD_LOCAL;
                }
            } else {
                if (frequencyMap[i].status == FREQUENCY_BAD_LOCAL) {
                    frequencyMap[i].status = FREQUENCY_BAD_BOTH;
                } else {
                    frequencyMap[i].status = FREQUENCY_BAD_EXTERNAL;
                }
            }
            break;
        }
    }
}
