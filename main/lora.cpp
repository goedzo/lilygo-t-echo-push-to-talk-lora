#include "utilities.h"
#include <stdint.h>
#include "settings.h"  // Include settings.h to use global variables

#include <RadioLib.h>
#include "lora.h"



// Maak eerst een Module-object aan met de juiste pinconfiguratie
Module module = Module(/*CS=*/10, /*DIO1=*/2, /*NRST=*/3, /*BUSY=*/9);

// Geef het Module-object door aan de SX1262 constructor
SX1262 radio = SX1262(&module);

void setupLoRa() {
    Serial.begin(9600);
    while (!Serial);

    Serial.print(F("[SX1262] Initializing ... "));

    // Initializeer de radio
    int state = radio.begin(868.0);  // Stel de juiste frequentie in
    if (state == RADIOLIB_ERR_NONE) {
        Serial.println(F("success!"));
    } else {
        Serial.print(F("failed, code "));
        Serial.println(state);
        while (true);  // Stop met uitvoeren als de initialisatie faalt
    }

    // Stel de spreading factor in met de opgegeven waarde
    state = radio.setSpreadingFactor(spreading_factor);
    if (state == RADIOLIB_ERR_NONE) {
        Serial.print(F("Spreading factor set to SF"));
        Serial.println(spreading_factor);
    } else {
        Serial.print(F("Failed to set spreading factor, code "));
        Serial.println(state);
    }

    // Stel het zendvermogen in (tussen -17 en 22 dBm)
    if (radio.setOutputPower(22) == RADIOLIB_ERR_INVALID_OUTPUT_POWER) {
        Serial.println(F("Selected output power is invalid for this module!"));
        while (true);
    }

    // Stel de stroomlimiet in (tussen 45 en 240 mA)
    if (radio.setCurrentLimit(80) == RADIOLIB_ERR_INVALID_CURRENT_LIMIT) {
        Serial.println(F("Selected current limit is invalid for this module!"));
        while (true);
    }

    Serial.println(F("LoRa setup completed successfully!"));
}

void sendPacket(uint8_t* pkt_buf, uint16_t len) {
    int state = radio.transmit(pkt_buf, len);
    if (state == RADIOLIB_ERR_NONE) {
        Serial.println(F("Transmission successful!"));
    } else {
        Serial.print(F("Transmission failed, code "));
        Serial.println(state);
    }
}

int receivePacket(uint8_t* pkt_buf, uint16_t max_len) {
    int state = radio.receive(pkt_buf, max_len);
    if (state == RADIOLIB_ERR_NONE) {
        Serial.println(F("Received packet successfully!"));
        return max_len;  // Of de werkelijke grootte van het ontvangen pakket
    } else {
        Serial.print(F("Receive failed, code "));
        Serial.println(state);
        return 0;
    }
}