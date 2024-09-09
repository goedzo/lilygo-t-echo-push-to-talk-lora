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

SX1262 radio = nullptr;       //SX1262
SPIClass        *rfPort    = nullptr;

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

            //Serial.println("SENT COMPLETE");
            uint16_t irqStatus = radio.getIrqStatus();
            if (irqStatus & RADIOLIB_SX126X_IRQ_TX_DONE) {
              //Serial.println(F("Transmission successful!"));
            }
            else {
              Serial.println(F("Transmission not done!!"));
            }


            int state = radio.finishTransmit();

            if (state == RADIOLIB_ERR_NONE) {
                // We have sent a package, so listen again
            } 
            else {
              Serial.print(F("Sent failed, code "));
              char buf[50];
              snprintf(buf, sizeof(buf), "Sent Err: %d", state);
              showError(buf);
              Serial.println(state);
            }

            //Let's reset lora to get receiving again. There is a bug when using a spread factor >10, receiving stops working
            radio.sleep(true);
            radio.standby();
            radio.startReceive(); //Start after processing, otherwise the packet is cleared before reading
            transmitFlag=false;        
        }
        else {
            //Serial.println("RECEIVE COMPLETE");
            handlePacket();
            transmitFlag=false;
            radio.startReceive(); //Start after processing, otherwise the packet is cleared before reading
        }
    }
}

bool setupLoRa() {
    rfPort = new SPIClass(
        /*SPIPORT*/NRF_SPIM3,
        /*MISO*/ LoRa_Miso,
        /*SCLK*/LoRa_Sclk,
        /*MOSI*/LoRa_Mosi);
    rfPort->begin();

    SPISettings spiSettings;

    radio = new Module(LoRa_Cs, LoRa_Dio1, LoRa_Rst, LoRa_Busy, *rfPort, spiSettings);
    // radio = new SX1262(new Module(_PINNUM(0,24), _PINNUM(0,20), _PINNUM(0,25), _PINNUM(0,17), *rfPort, spiSettings));


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

    // set the function that will be called
    // when new packet is received
    operationDone=false;
    radio.setDio1Action(setFlag);


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
    if(transmitFlag) {
        //We are already transmitting. Avoid flooding
        char buf[50];
        showError("Already in transmit, skipping");
        return;
    }

    // Start non-blocking transmission
    int state = radio.startTransmit(pkt_buf, len);
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
    int state = radio.sleep();
    if (state == RADIOLIB_ERR_NONE) {
        Serial.println(F("LoRa module is now in sleep mode."));
    } else {
        char buf[50];
        snprintf(buf, sizeof(buf), "LoRa Sleep Error: %d", state);
        showError(buf);  // Show error for LoRa sleep failure
    }
}
