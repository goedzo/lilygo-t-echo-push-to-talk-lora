#include <LoRa.h>
#include "lora.h"

// Pin configuration for NRF52840
#define LORA_SCK 29
#define LORA_MISO 28
#define LORA_MOSI 27
#define LORA_CS 26
#define LORA_RST 25
#define LORA_IRQ 24
#define LORA_BUSY 23

// Create a Radio object
SX1262 radio = new Module(LORA_CS, LORA_IRQ, LORA_RST, LORA_BUSY);

void setupLoRa() {
    int state = radio.begin();
    if (state != RADIOLIB_ERR_NONE) {
        Serial.print(F("Failed to initialize LoRa module, code: "));
        Serial.println(state);
        while (true);
    }

    // Set output power to 22 dBm (valid range is -17 to 22 dBm)
    if (radio.setOutputPower(22) == RADIOLIB_ERR_INVALID_OUTPUT_POWER) {
        Serial.println(F("Selected output power is invalid for this module!"));
        while (true);
    }

    // Set overcurrent protection limit to 80 mA (valid range is 45 to 240 mA)
    if (radio.setCurrentLimit(80) == RADIOLIB_ERR_INVALID_CURRENT_LIMIT) {
        Serial.println(F("Selected current limit is invalid for this module!"));
        while (true);
    }

    Serial.println(F("LoRa setup completed successfully!"));
}

void sendPacket(uint8_t* pkt_buf, uint16_t len) {
    radio.beginPacket();
    radio.write(pkt_buf, len);
    radio.endPacket();
}

int receivePacket(uint8_t* pkt_buf, uint16_t max_len) {
    int pkt_size = radio.parsePacket();
    if (pkt_size > 0) {
        radio.readBytes(pkt_buf, pkt_size < max_len ? pkt_size : max_len);
    }
    return pkt_size;
}