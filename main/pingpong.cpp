#include "pingpong.h"
#include "display.h"
#include "utilities.h"

// Initialize global variables
SX1262* p_radio = nullptr;
SPIClass* p_rfPort = nullptr;
int transmissionState = RADIOLIB_ERR_NONE;
bool p_transmitFlag = false;
volatile bool p_operationDone = false;

// This function is called when a complete packet is transmitted or received by the module
// IMPORTANT: this function MUST be 'void' type and MUST NOT have any arguments!
void p_setFlag(void) {
  // We sent or received a packet, set the flag
  p_operationDone = true;
}

void setupPingPong() {

  // Initialize SX1262 with default settings
  Serial.print(F("[SX1262] Initializing Ping Pong ... "));

  p_rfPort = new SPIClass(
      /*SPIPORT*/NRF_SPIM3,
      /*MISO*/ LoRa_Miso,
      /*SCLK*/LoRa_Sclk,
      /*MOSI*/LoRa_Mosi);
  p_rfPort->begin();

  SPISettings spiSettings;

  // SX1262 has the following connections:
  // NSS pin:   10
  // DIO1 pin:  2
  // NRST pin:  3
  // BUSY pin:  9
  p_radio = new SX1262(new Module(LoRa_Cs, LoRa_Dio1, LoRa_Rst, LoRa_Busy, *p_rfPort, spiSettings));

  int state = p_radio->begin();
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("success!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
    while (true);
  }

  // Set the function that will be called when new packet is received
  p_radio->setDio1Action(p_setFlag);

  state = p_radio->setSpreadingFactor(deviceSettings.spreading_factor);
  if (state == RADIOLIB_ERR_NONE) {
      Serial.print(F("Spreading factor set to SF"));
      Serial.println(deviceSettings.spreading_factor);
  } else {
      Serial.print(F("Failed to set spreading factor, code "));
      Serial.println(state);
  }

  if (p_radio->setOutputPower(22) == RADIOLIB_ERR_INVALID_OUTPUT_POWER) {
      Serial.println(F("Selected output power is invalid for this module!"));
  }

  // Stel de stroomlimiet in (tussen 45 en 240 mA)
  if (p_radio->setCurrentLimit(80) == RADIOLIB_ERR_INVALID_CURRENT_LIMIT) {
      Serial.println(F("Selected current limit is invalid for this module!"));
  }


  // Start listening for LoRa packets on this node
  Serial.print(F("[SX1262] Starting to listen ... "));
  state = p_radio->startReceive();
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("success!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
    while (true);
  }
}

void pingpongStart() {
    Serial.print(F("[SX1262] Sending first packet ... "));
    transmissionState = p_radio->startTransmit("Ping!");
    p_transmitFlag = true;
    updDisp(5, "Ping!",true);

}


void pingpongLoop() {
  // Check if the previous operation finished
  if (p_operationDone) {
    // Reset flag
    p_operationDone = false;

    if (p_transmitFlag) {
      // The previous operation was transmission, listen for response
      if (transmissionState == RADIOLIB_ERR_NONE) {
        Serial.println(F("transmission finished!"));
        updDisp(5, " ",true);
      } else {
        Serial.print(F("failed, code "));
        Serial.println(transmissionState);
      }

      // Listen for response
      p_radio->startReceive();
      p_transmitFlag = false;

    } else {
      // The previous operation was reception
      String str;
      int state = p_radio->readData(str);

      if (state == RADIOLIB_ERR_NONE) {
        // Packet was successfully received
        Serial.println(F("[SX1262] Received packet!"));

        updDisp(5, "       pong",false);


        // Print data of the packet
        Serial.print(F("[SX1262] Data:\t\t"));
        Serial.println(str);

        // Print RSSI (Received Signal Strength Indicator)
        Serial.print(F("[SX1262] RSSI:\t\t"));
        Serial.print(p_radio->getRSSI());
        Serial.println(F(" dBm"));

        // Print SNR (Signal-to-Noise Ratio)
        Serial.print(F("[SX1262] SNR:\t\t"));
        Serial.print(p_radio->getSNR());
        Serial.println(F(" dB"));

        char display_msg[30];
        snprintf(display_msg, sizeof(display_msg), "SNR: %.3f  dB", p_radio->getSNR() );
        updDisp(6, display_msg,false);
        snprintf(display_msg, sizeof(display_msg), "RSSI: %.3f dBm", p_radio->getRSSI() );
        updDisp(7, display_msg,true);


      }

      // Send another packet
      Serial.print(F("[SX1262] Sending another packet ... "));
      updDisp(5, "Ping!",true);
      transmissionState = p_radio->startTransmit("Ping!");
      p_transmitFlag = true;
    }
  }
}
