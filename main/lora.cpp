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

#define PACKET_BUFFER_SIZE 5
#define NEWER_PACKET_QUEUE_SIZE 5

PacketQueue newerPacketQueue[NEWER_PACKET_QUEUE_SIZE];
int newerPacketQueueCount = 0;


PacketBuffer packetBuffer[PACKET_BUFFER_SIZE];
int bufferIndex = 0;


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
int FrequencyHopSeconds = 47; //After how many seconds do we hop to the next frequency?

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

unsigned int messageCounter = 0;       // The counter I add to each message so that it can be tracket
unsigned int lastReceivedCounter = 0;  // Global variable to store the last received packet counter
uint8_t* lastMessageBuffer = nullptr;  // Buffer to store the last message sent
uint16_t lastMessageLength = 0;        // Length of the last message sent

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
// Pseudo-random frequency hopping based on a shared time source and randomness
float getNextFrequency(unsigned long sharedTime, unsigned long sharedSeed) {
    // Calculate how many intervals of 'FrequencyHopSeconds' have passed
    unsigned long intervalCount = sharedTime / FrequencyHopSeconds;  // Count of hop intervals passed

    // Combine intervalCount and sharedSeed for randomness
    unsigned long randomValue = intervalCount ^ sharedSeed;  // XOR interval count with the seed for randomness
    Serial.print(F("IntervalCount: ")); 
    Serial.println(intervalCount);
    Serial.print(F("SharedSeed: "));
    Serial.println(sharedSeed);
    Serial.print(F("RandomValue: "));
    Serial.println(randomValue);

    // Generate a random starting index based on intervalCount
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

void storePacketInQueue(uint8_t* pkt_buf, uint16_t len, unsigned int counter) {
    if (newerPacketQueueCount < NEWER_PACKET_QUEUE_SIZE) {
        newerPacketQueue[newerPacketQueueCount].packetData = new uint8_t[len];
        memcpy(newerPacketQueue[newerPacketQueueCount].packetData, pkt_buf, len);
        newerPacketQueue[newerPacketQueueCount].packetLen = len;
        newerPacketQueue[newerPacketQueueCount].packetCounter = counter;
        newerPacketQueueCount++;
    } else {
        Serial.println(F("Newer packet queue full, discarding packet."));
    }
}

void processPacketQueue() {
    // Sort the packets by counter (optional for ordered processing)
    for (int i = 0; i < newerPacketQueueCount; i++) {
        Packet packet;
        if (packet.parsePacket(newerPacketQueue[i].packetData, newerPacketQueue[i].packetLen)) {
            Serial.print(F("Processing queued packet: "));
            Serial.println(packet.packetCounter);
            handlePacket(packet);  // Process the packet
        }
        // Free the memory after processing
        delete[] newerPacketQueue[i].packetData;
    }
    newerPacketQueueCount = 0;  // Reset the queue count after processing
}


bool checkForMissingPackets(Packet& packet, uint8_t* rcv_pkt_buf, uint16_t packet_len) {
    unsigned int expectedPacketCounter = lastReceivedCounter + 1;
    unsigned int missedPackets = packet.packetCounter - lastReceivedCounter - 1;

    if (lastReceivedCounter > 0 && missedPackets > 0) {
        // Check if we missed more than PACKET_BUFFER_SIZE packets
        if (missedPackets > PACKET_BUFFER_SIZE) {
            Serial.println(F("Too many packets missed, unable to request older packets"));
            lastReceivedCounter = packet.packetCounter;  // Skip to the newest packet
            return true;  // Continue processing since no retransmission request is needed
        } else {
            // Request all missed packets sequentially
            for (unsigned int missedCounter = expectedPacketCounter; missedCounter < packet.packetCounter; missedCounter++) {
                char requestBuf[10];
                snprintf(requestBuf, sizeof(requestBuf), "REQ%04u", missedCounter);
                sendPacket((uint8_t*)requestBuf, strlen(requestBuf));
                Serial.print(F("Requesting retransmission of packet: "));
                Serial.println(missedCounter);
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
    Serial.println(F("Retransmission request completed, processing queued packets..."));

    // Process all queued packets, in order, as long as they are in sequence
    bool packetProcessed = true;
    while (packetProcessed) {
        packetProcessed = false;
        for (int i = 0; i < newerPacketQueueCount; i++) {
            // Check if the next expected packet is in the queue
            if (newerPacketQueue[i].packetCounter == lastReceivedCounter + 1) {
                // Process the packet
                Packet packet;
                if (packet.parsePacket(newerPacketQueue[i].packetData, newerPacketQueue[i].packetLen)) {
                    Serial.print(F("Processing queued packet: "));
                    Serial.println(packet.packetCounter);
                    handlePacket(packet);  // Process the packet

                    // Update the last received counter
                    lastReceivedCounter = packet.packetCounter;
                    packetProcessed = true;
                }

                // Remove the packet from the queue and free memory
                delete[] newerPacketQueue[i].packetData;

                // Shift the remaining packets in the queue
                for (int j = i; j < newerPacketQueueCount - 1; j++) {
                    newerPacketQueue[j] = newerPacketQueue[j + 1];
                }
                newerPacketQueueCount--;

                // Break to recheck the queue after processing
                break;
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
                // Resend the last message after hopping to the new frequency
                if (lastMessageBuffer && lastMessageLength > 0) {
                    delay(1000); //To allow some deviation in the other devices
                    Serial.println(F("Resending last message after frequency hop"));
                    sendPacket(lastMessageBuffer, lastMessageLength);
                }
            }
            else {
                radio->startReceive();  // Start receiving after transmission

            }
        } else {
            uint16_t packet_len = radio->getPacketLength(false);
            //uint16_t irqStatus = radio->getIrqFlags();  //Radiolib 7.0.0 support, but not working
            uint16_t irqStatus = radio->getIrqStatus();
            unsigned char rcv_pkt_buf[MAX_PKT];

            if (irqStatus & RADIOLIB_SX126X_IRQ_RX_DONE) {
                //radio->clearIrqFlags(irqStatus); //Radiolib 7.0.0 support, but not working

                memset(rcv_pkt_buf, 0, MAX_PKT);  // Clear the receive buffer
                int state = radio->readData(rcv_pkt_buf, packet_len);
                //Quickly continue receiving
                radio->startReceive();
                if (state == RADIOLIB_ERR_NONE) {
                    rcv_pkt_buf[packet_len] = '\0';  // Null-terminate the received packet

                    Packet packet;
                    if (packet.parsePacket(rcv_pkt_buf, packet_len)) {
                        Serial.print(F("Packet parsed: "));
                        Serial.println(packet.type);
                        Serial.println(packet.content);
                        Serial.println(packet.packetCounter);

                        // Call checkForMissingPackets here
                        if(checkForMissingPackets(packet, rcv_pkt_buf, packet_len)) {
                            //True = ok to process. Nothing is missing
                            handlePacket(packet);
                        }


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
                //radio->startReceive();  // Prepare to receive the next packet
            }
        }
    }

    // Handle frequency hopping using RTC time (hours, minutes, and seconds)
    if (deviceSettings.frequency_hopping_enabled && time_set) {
        RTC_Date currentTime = rtc.getDateTime();  // Get current time from the RTC
        unsigned long sharedTime = currentTime.hour * 3600 + currentTime.minute * 60 + currentTime.second;

        if (sharedTime != lastHopTime) {  // Adjust the hop interval if needed


            // Use shared time to get the next frequency using the frequency hopping algorithm
            float newFrequency = getNextFrequency(sharedTime, sharedSeed);
            if(newFrequency==currentFrequency ) {
                //No Need to Change
                lastHopTime = sharedTime;
            }
            else {
                // Check for lost synchronization
                if (isSyncLost()) {
                    Serial.println(F("Sync lost, reinitializing synchronization..."));
                    // Reinitialize sync using the current RTC time
                    lastMapShareTime = sharedTime;
                    syncLossTimer = millis();  // Reset the sync loss timer

                    lastHopTime = sharedTime;  // Update the last hop time

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
                
                lastHopTime = sharedTime;  // Update the last hop time

            }
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
    // Free previous buffer data
    if (packetBuffer[bufferIndex].packetData) {
        delete[] packetBuffer[bufferIndex].packetData;
    }

    // Store new packet
    packetBuffer[bufferIndex].packetData = new uint8_t[len];
    memcpy(packetBuffer[bufferIndex].packetData, pkt_buf, len);
    packetBuffer[bufferIndex].packetLen = len;
    packetBuffer[bufferIndex].messageCounter = counter;

    bufferIndex = (bufferIndex + 1) % PACKET_BUFFER_SIZE;  // Circular increment
}

void handleRetransmitRequest(unsigned int requestedCounter) {
    for (int i = 0; i < PACKET_BUFFER_SIZE; i++) {
        if (packetBuffer[i].messageCounter == requestedCounter) {
            Serial.print(F("Resending packet with counter: "));
            Serial.println(requestedCounter);
            sendPacket(packetBuffer[i].packetData, packetBuffer[i].packetLen);
            return;
        }
    }
    Serial.println(F("Requested packet not found in buffer"));
}

void sendPacket(uint8_t* pkt_buf, uint16_t len) {
    if (transmitFlag) {
        showError("Already in transmit, skipping");
        transmitFlag = false;
        return;
    }

    // Increment the message counter for each new packet
    messageCounter++;


    // The first 3 characters in pkt_buf are the packet type
    char typeBuffer[4];
    strncpy(typeBuffer, (char*)pkt_buf, 3);  // Extract the first 3 characters (packet type)
    typeBuffer[3] = '\0';

    // Construct the rest of the header starting after the packet type
    String header = String(typeBuffer);  // Initialize with the packet type

    // Add message counter with the "~PC" field
    header += "~PC" + String(messageCounter);  // Ensure 4 digits for messageCounter

    // Add sendDateTime from RTC
    header += "~SD" + getFormattedDateTime();  // Example: "20230921183045"

    // Optionally add GPS data if available
    /*
    if (gpsDataAvailable()) {
        header += "~GP" + getGPSData();  // Ensure GPS data is formatted properly (12 characters)
    }
    */

    header += "~~"; //Always end with ~~ before the actual content

    // Calculate the new length (header + the remaining content after the first 3 characters)
    uint16_t headerLen = header.length();
    uint16_t newLen = headerLen + (len - 3);  // Total length minus the first 3 characters of pkt_buf

    // Create a dynamically sized buffer for the new packet
    char* send_pkt_buf = new char[newLen + 1];  // +1 for null terminator

    // Copy the header into the new buffer
    strcpy(send_pkt_buf, header.c_str());

    // Append the rest of the original content (after the first 3 characters)
    memcpy(send_pkt_buf + headerLen, pkt_buf + 3, len - 3);
    send_pkt_buf[newLen] = '\0';  // Null-terminate the packet

    // Store the last message
    if (lastMessageBuffer) {
        delete[] lastMessageBuffer;  // Free the previous buffer
    }
    lastMessageBuffer = new uint8_t[newLen];
    memcpy(lastMessageBuffer, send_pkt_buf, newLen);
    lastMessageLength = newLen;

    // Get time-on-air for logging
    timeOnAir = radio->getTimeOnAir(newLen);
    Serial.print(F("Time-on-Air (ms): "));
    Serial.println(timeOnAir);

    Serial.println(send_pkt_buf);

    storePacketInBuffer((uint8_t*)send_pkt_buf, newLen, messageCounter);  // Store in buffer in case we need to resend

    // Start the transmission
    int state = radio->startTransmit((uint8_t*)send_pkt_buf, newLen);
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

// Check if the packet is duplicate by comparing packetCounter
bool isDuplicatePacket(Packet& packet) {
    if (packet.packetCounter == lastReceivedCounter) {
        return true;  // Duplicate packet
    }
    lastReceivedCounter = packet.packetCounter;  // Update the last received counter
    return false;
}
