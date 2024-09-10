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

SX1262* radio = nullptr;
SPIClass* rfPort = nullptr;


// flag to indicate that a packet was sent or received
volatile bool operationDone = false;
// flag to indicate transmission or reception state
bool transmitFlag = false;

void setFlag(void) {
  // we sent or received a packet, set the flag
  operationDone = true;
  //Serial.println("setFlag - Lora Action complete");
}

void checkLoraPacketComplete(){
    if(operationDone) {
        //Serial.println("checkLoraPacketComplete - Lora Action complete");
        // reset flag
        operationDone = false;
        if(transmitFlag) {
            int state = radio->finishTransmit();
            if (state == RADIOLIB_ERR_NONE) {
                // We have sent a package sucessfull!
            } 
            else {
              Serial.print(F("Sent failed, code "));
              char buf[50];
              snprintf(buf, sizeof(buf), "Sent Err: %d", state);
              showError(buf);
              Serial.println(state);
            }
            radio->startReceive();
            transmitFlag = false;
        }
        else {
            //Serial.println("RECEIVE COMPLETE");
            handlePacket();
            operationDone=false;
        }
    }
}

bool setupLoRa() {
  // Initialize SX1262 with default settings
  Serial.print(F("Initializing Lora ... "));

  rfPort = new SPIClass(
      /*SPIPORT*/NRF_SPIM3,
      /*MISO*/ LoRa_Miso,
      /*SCLK*/LoRa_Sclk,
      /*MOSI*/LoRa_Mosi);
  rfPort->begin();

  SPISettings spiSettings;

  // SX1262 has the following connections:
  // NSS pin:   10
  // DIO1 pin:  2
  // NRST pin:  3
  // BUSY pin:  9
  radio = new SX1262(new Module(LoRa_Cs, LoRa_Dio1, LoRa_Rst, LoRa_Busy, *rfPort, spiSettings));

  int state = radio->begin();
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("success!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
    while (true);
  }

  // Set the function that will be called when new packet is received
  radio->setDio1Action(setFlag);

  state = radio->setSpreadingFactor(deviceSettings.spreading_factor);
  if (state == RADIOLIB_ERR_NONE) {
      Serial.print(F("Spreading factor set to SF"));
      Serial.println(deviceSettings.spreading_factor);
  } else {
      Serial.print(F("Failed to set spreading factor, code "));
      Serial.println(state);
  }

  if (radio->setOutputPower(18) == RADIOLIB_ERR_INVALID_OUTPUT_POWER) {
      Serial.println(F("Selected output power is invalid for this module!"));
  }

  // Stel de stroomlimiet in (tussen 45 en 240 mA)
  if (radio->setCurrentLimit(90) == RADIOLIB_ERR_INVALID_CURRENT_LIMIT) {
      Serial.println(F("Selected current limit is invalid for this module!"));
  }


    // Start listening for LoRa packets on this node
      Serial.println(F("LoRa setup completed successfully!"));
      state = radio->startReceive();
    if (state == RADIOLIB_ERR_NONE) {
      Serial.println(F("success!"));
    } else {
      Serial.print(F("failed, code "));
      Serial.println(state);
    }


    return true;
}


void sendPacket(uint8_t* pkt_buf, uint16_t len) {
    if(transmitFlag) {
        //We are already transmitting. Avoid flooding
        char buf[50];
        showError("Already in transmit, skipping");
        return;
    }

    // Start non-blocking transmission
    int state = radio->startTransmit(pkt_buf, len);
    transmitFlag = true;

    if (state != RADIOLIB_ERR_NONE) {
        Serial.print(F("Transmission start failed, code "));
        Serial.println(state);
        char buf[50];
        snprintf(buf, sizeof(buf), "Lora Strt Trnsmt Err: %d", state);
        showError(buf);
        //Let's reinitialize the radio
        setupLoRa();
        return;
    }
}

void sendPacket(const char* str) {
    if(transmitFlag) {
        //We are already transmitting. Avoid flooding
        char buf[50];
        showError("Already in transmit, skipping");
        return;
    }

    // Start non-blocking transmission
    int state = radio->startTransmit(str);
    transmitFlag = true;

    if (state != RADIOLIB_ERR_NONE) {
        Serial.print(F("Transmission start failed, code "));
        Serial.println(state);
        char buf[50];
        snprintf(buf, sizeof(buf), "Lora Strt Trnsmt Err: %d", state);
        showError(buf);
        //Let's reinitialize the radio
        setupLoRa();
        return;
    }
}

int receivePacket(uint8_t* pkt_buf, uint16_t max_len) {

    // Get the length of the received packet
    uint16_t packet_len = radio->getPacketLength(false);
    
    if(packet_len==0) {
        return 0;
    }

    if (packet_len > max_len) {
        // Prevent buffer overflow if the packet is larger than the provided buffer
        packet_len = max_len;
    }

    //Let's check the IRQ status to make sure all data is actually received
    uint16_t irqStatus = radio->getIrqStatus();
    if (irqStatus & RADIOLIB_SX126X_IRQ_RX_DONE) {
        // Read the data into the buffer
        int state = radio->readData(pkt_buf, packet_len);

        if (state == RADIOLIB_ERR_NONE) {
            return packet_len;  // Of de werkelijke grootte van het ontvangen pakket
        } else {
            if (state == RADIOLIB_ERR_RX_TIMEOUT ) {
                //This is OK, no data was received
            }
            else {
                Serial.print(F("Receive failed, code "));
                char buf[50];
                snprintf(buf, sizeof(buf), "Receive Err: %d", state);
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

void sleepLoRa() {
    // Put the LoRa module into sleep mode using RadioLib's sleep function
    int state = radio->sleep();
    if (state == RADIOLIB_ERR_NONE) {
        Serial.println(F("LoRa module is now in sleep mode."));
    } else {
        char buf[50];
        snprintf(buf, sizeof(buf), "LoRa Sleep Error: %d", state);
        showError(buf);  // Show error for LoRa sleep failure
    }
}
