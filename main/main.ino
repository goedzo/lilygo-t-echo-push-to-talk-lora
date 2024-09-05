#include <Arduino.h>
#include <stdint.h>

#include "settings.h"
#include "app_modes.h"
#include "display.h"
#include "lora.h"
#include "audio.h"

void setup() {
    setupDisplay();
    setupLoRa();
    //setupI2S();
    setupSettings();
    updModeAndChannelDisplay();
}

void loop() {
    handleAppModes();
}