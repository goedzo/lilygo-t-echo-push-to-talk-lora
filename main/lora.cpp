#include "delay.h"
#include "utilities.h"
#include <SPI.h>
#include <GxEPD.h>
#include <GxDEPG0150BN/GxDEPG0150BN.h>  // 1.54" b/w
#include <RadioLib.h>
#include <stdint.h>
#include "settings.h"  // Include settings.h to use global variables
#include "display.h"
#include "app_modes.h"  // Include settings.h to use global variables
#include "lora.h"
#include "packet.h"

SX1262* radio = nullptr;
SPIClass* rfPort = nullptr;

float currentFrequency = 869.47;  // Default frequency

// Flag to indicate that a packet was sent or received
volatile bool operationDone = false;
bool transmitFlag = false;
size_t timeOnAir = 0;

// Define frequency hopping related variables here
float startFreq = 863.0; //869.46;
float endFreq = 869.65; //869.48;
float stepSize = 0.05;//0.01;


int numFrequencies = (endFreq - startFreq) / stepSize;  // Calculate number of frequencies at runtime
FrequencyStatus* frequencyMap = nullptr;  // Pointer to store the frequency map

void initializeFrequencyMap() {
    // Dynamically allocate the frequency map array
    frequencyMap = new FrequencyStatus[numFrequencies];
    for (int i = 0; i < numFrequencies; i++) {
        frequencyMap[i].frequency = startFreq + (i * stepSize);
        frequencyMap[i].isGood = true;  // Start by assuming all frequencies are good
    }
}

void setFlag(void) {
    operationDone = true;  // Set the flag when a packet is sent or received
}

void checkLoraPacketComplete() {
    if (operationDone) {
        operationDone = false;

        if (transmitFlag) {
            int state = radio->finishTransmit();
            if (state == RADIOLIB_ERR_NONE) {
                Serial.println("Packet was Sent, finishTransmit");
            } else {
                Serial.print(F("Sent failed, code "));
                char buf[50];
                snprintf(buf, sizeof(buf), "Sent Err: %d", state);
                showError(buf);
                Serial.println(state);
            }
            radio->startReceive();  // Start receiving after transmission
            transmitFlag = false;
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

                        // *** Channel Quality Evaluation ***
                        float rssi = radio->getRSSI();
                        float snr = radio->getSNR();
                        int qualityScore = calculateQuality(rssi, snr, false);

                        if (qualityScore >= QUALITY_THRESHOLD) {
                            markFrequencyAsGood(currentFrequency);
                        } else {
                            markFrequencyAsBad(currentFrequency);
                        }
                    } else {
                        Serial.println("Error packet received");
                    }
                } else if (state != RADIOLIB_ERR_RX_TIMEOUT) {
                    Serial.print(F("Receive failed, code "));
                    char buf[50];
                    snprintf(buf, sizeof(buf), "Receive Err: %d", state);
                    showError(buf);
                    Serial.println(state);
                }
            }
            radio->startReceive();  // Prepare to receive the next packet
        }
    }
}

bool setupLoRa() {
    Serial.print(F("Initializing Lora ... "));

    transmitFlag = false;
    operationDone = false;

    rfPort = new SPIClass(
        /*SPIPORT*/NRF_SPIM3,
        /*MISO*/ LoRa_Miso,
        /*SCLK*/LoRa_Sclk,
        /*MOSI*/LoRa_Mosi
    );
    rfPort->begin();
    SPISettings spiSettings;

    radio = new SX1262(new Module(LoRa_Cs, LoRa_Dio1, LoRa_Rst, LoRa_Busy, *rfPort, spiSettings));

    int state = radio->begin(currentFrequency);  // Use currentFrequency
    if (state == RADIOLIB_ERR_NONE) {
        Serial.println(F("success!"));
    } else {
        Serial.print(F("failed, code "));
        Serial.println(state);
        while (true);
    }

    radio->setDio1Action(setFlag);  // Set the callback for packet transmission/reception

    // Set Spreading Factor, Bandwidth, Coding Rate
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

    Serial.println(F("LoRa setup completed successfully!"));
    return radio->startReceive() == RADIOLIB_ERR_NONE;
}

void sendPacket(uint8_t* pkt_buf, uint16_t len) {
    if (transmitFlag) {
        showError("Already in transmit, skipping");
        transmitFlag = false;
        return;
    }

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
}

void sendPacket(const char* str) {
    if (transmitFlag) {
        showError("Already in transmit, skipping");
        return;
    }

    timeOnAir = radio->getTimeOnAir(strlen(str));
    Serial.print(F("Time-on-Air (ms): "));
    Serial.println(timeOnAir);

    int state = radio->startTransmit(str);
    transmitFlag = true;

    if (state != RADIOLIB_ERR_NONE) {
        Serial.print(F("Transmission start failed, code "));
        Serial.println(state);
        char buf[50];
        snprintf(buf, sizeof(buf), "Lora Strt Trnsmt Err: %d", state);
        showError(buf);
        setupLoRa();  // Reinitialize the radio
    }
}

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

void markFrequencyAsGood(float freq) {
    for (int i = 0; i < numFrequencies; i++) {
        if (frequencyMap[i].frequency == freq) {
            frequencyMap[i].isGood = true;
            break;
        }
    }
}

void markFrequencyAsBad(float freq) {
    for (int i = 0; i < numFrequencies; i++) {
        if (frequencyMap[i].frequency == freq) {
            frequencyMap[i].isGood = false;
            break;
        }
    }
}

// New function to set frequency
int setFrequency(float freq) {
    int state = radio->setFrequency(freq);  // Set frequency on the radio

    if (state == RADIOLIB_ERR_NONE) {
        currentFrequency = freq;  // Update the global frequency variable if successful
    } else {
        Serial.print(F("Failed to set frequency, code "));
        Serial.println(state);
    }

    return state;  // Return the state (success or error code)
}