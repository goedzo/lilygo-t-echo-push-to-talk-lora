#include "utilities.h"
#include <SPI.h>
#include <GxEPD.h>
#include <GxDEPG0150BN/GxDEPG0150BN.h>  // 1.54" b/w
#include <RadioLib.h>

#include <stdint.h>
#include "settings.h"  // Include settings.h to use global variables
#include "display.h"
#include "lora.h"

SX1262 radio = nullptr;       //SX1262
SPIClass        *rfPort    = nullptr;


bool setupLoRa() {
    rfPort = new SPIClass(
        /*SPIPORT*/NRF_SPIM3,
        /*MISO*/ LoRa_Miso,
        /*SCLK*/LoRa_Sclk,
        /*MOSI*/LoRa_Mosi);
    rfPort->begin();

    SPISettings spiSettings;

    radio = new Module(LoRa_Cs, LoRa_Dio1, LoRa_Rst, LoRa_Busy, *rfPort, spiSettings);

    SerialMon.print("[SX1262] Initializing ...  ");
    // carrier frequency:           868.0 MHz
    // bandwidth:                   125.0 kHz
    // spreading factor:            9
    // coding rate:                 7
    // sync word:                   0x12 (private network)
    // output power:                14 dBm
    // current limit:               60 mA
    // preamble length:             8 symbols
    // TCXO voltage:                1.6 V (set to 0 to not use TCXO)
    // regulator:                   DC-DC (set to true to use LDO)
    // CRC:                         enabled
    int state = radio.begin(868.0);
    if (state != ERROR_NONE) {
        SerialMon.print(("failed, code "));
        SerialMon.println(state);

        char buf[30];
        snprintf(buf, sizeof(buf), "Lora failed to init: %d", state);
        showError(buf);
        return false;
    }


    // Stel de spreading factor in met de opgegeven waarde
    state = radio.setSpreadingFactor(deviceSettings.spreading_factor);
    if (state == RADIOLIB_ERR_NONE) {
        Serial.print(F("Spreading factor set to SF"));
        Serial.println(deviceSettings.spreading_factor);
    } else {
        Serial.print(F("Failed to set spreading factor, code "));
        Serial.println(state);
    }

    // Stel het zendvermogen in (tussen -17 en 22 dBm)
    if (radio.setOutputPower(22) == RADIOLIB_ERR_INVALID_OUTPUT_POWER) {
        Serial.println(F("Selected output power is invalid for this module!"));
        return false;
    }

    // Stel de stroomlimiet in (tussen 45 en 240 mA)
    if (radio.setCurrentLimit(80) == RADIOLIB_ERR_INVALID_CURRENT_LIMIT) {
        Serial.println(F("Selected current limit is invalid for this module!"));
        return false;
    }


    // Start non-blocking receive
    radio.startReceive();

    Serial.println(F("LoRa setup completed successfully!"));
    return true;
}

void sendPacket(uint8_t* pkt_buf, uint16_t len) {
    int state = radio.transmit(pkt_buf, len);
    if (state == RADIOLIB_ERR_NONE) {
        Serial.println(F("Transmission successful!"));
    } else {
        Serial.print(F("Transmission failed, code "));
        Serial.println(state);
        char buf[50];
        snprintf(buf, sizeof(buf), "Lora Sent Error: %d", state);
        showError(buf);


    }
    //Allow receiving of messages
    radio.startReceive();
}

int receivePacket(uint8_t* pkt_buf, uint16_t max_len) {

    // Get the length of the received packet
    uint16_t packet_len = radio.getPacketLength(false);
    
    if(packet_len==0) {
        return 0;
    }

    if (packet_len > max_len) {
        // Prevent buffer overflow if the packet is larger than the provided buffer
        packet_len = max_len;
    }

    //Let's check the IRQ status to make sure all data is actually received
    uint16_t irqStatus = radio.getIrqStatus();
    if (irqStatus & RADIOLIB_SX126X_IRQ_RX_DONE) {
        // Read the data into the buffer
        int state = radio.readData(pkt_buf, packet_len);

        if (state == RADIOLIB_ERR_NONE) {
            // After receiving, re-enable receive mode
            radio.startReceive();
            return packet_len;  // Of de werkelijke grootte van het ontvangen pakket
        } else {
            if (state == RADIOLIB_ERR_RX_TIMEOUT ) {
                //This is OK, no data was received
            }
            else {
                Serial.print(F("Receive failed, code "));
                char buf[50];
                snprintf(buf, sizeof(buf), "Lora Receive Error: %d", state);
                showError(buf);
                Serial.println(state);
            }
            return 0;
        }
    }
    else {
      return 0;
    }
}


