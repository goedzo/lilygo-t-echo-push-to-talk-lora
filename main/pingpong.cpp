#include "pingpong.h"
#include "display.h"

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
      /*MISO*/ _PINNUM(0,23),
      /*SCLK*/_PINNUM(0,19),
      /*MOSI*/_PINNUM(0,22));
  p_rfPort->begin();

  SPISettings spiSettings;

  // SX1262 has the following connections:
  // NSS pin:   10
  // DIO1 pin:  2
  // NRST pin:  3
  // BUSY pin:  9
  p_radio = new SX1262(new Module(_PINNUM(0,24), _PINNUM(0,20), _PINNUM(0,25), _PINNUM(0,17), *p_rfPort, spiSettings));

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
  p_radio->setSpreadingFactor(12);

  #if defined(INITIATING_NODE)
    // Send the first packet on this node
    Serial.print(F("[SX1262] Sending first packet ... "));
    transmissionState = p_radio->startTransmit("Hello World!");
    p_transmitFlag = true;
  #else
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
  #endif
}

void pingpongStart() {
    Serial.print(F("[SX1262] Sending first packet ... "));
    transmissionState = p_radio->startTransmit("Hello World!");
    p_transmitFlag = true;
    updDisp(5, "ping",true);

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
        updDisp(6, display_msg);
        snprintf(display_msg, sizeof(display_msg), "RSSI: %.3f dBm", p_radio->getRSSI() );
        updDisp(7, display_msg);


      }

      // Send another packet
      Serial.print(F("[SX1262] Sending another packet ... "));
      updDisp(5, "ping",true);
      transmissionState = p_radio->startTransmit("Hello World!");
      p_transmitFlag = true;
    }
  }
}
