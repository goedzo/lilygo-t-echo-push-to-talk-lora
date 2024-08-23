#include <Arduino.h>
#include "display.h"
#include "app_modes.h"
#include "lora.h"
#include "audio.h"
#include "settings.h"

void setup() {
    setupDisplay();
    setupLoRa();
    setupI2S();
    setupSettings();
    updModeAndChannelDisplay();
}

void loop() {
    handleSettings();
    handleAppModes();
}