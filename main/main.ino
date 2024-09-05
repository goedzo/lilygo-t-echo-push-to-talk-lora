#include <Arduino.h>
#include <stdint.h>
#include <SPI.h>
#include <Wire.h>
#include <GxEPD.h>
#include <GxDEPG0150BN/GxDEPG0150BN.h> 
#include <TinyGPS++.h>
#include <GxIO/GxIO_SPI/GxIO_SPI.h>
#include <GxIO/GxIO.h>
#include <pcf8563.h>
#include <RadioLib.h>
#include <bluefruit.h>
#include <AceButton.h>


#include "settings.h"
#include "app_modes.h"
#include "display.h"
#include "lora.h"
#include "audio.h"

void setup() {
    Serial.println("Booting...");
    Serial.println("Init display");
    setupDisplay();
    Serial.println("Init lora");
    setupLoRa();
    //setupI2S();
    Serial.println("Init settings");
    setupSettings();
    updModeAndChannelDisplay();
}

void loop() {
    handleAppModes();
}