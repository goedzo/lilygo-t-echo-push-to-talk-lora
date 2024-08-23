#include "app_modes.h"
#include "display.h"
#include "lora.h"
#include "audio.h"

OperationMode current_mode = PTT;
char channels[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
int channel_idx = 0;
int volume_level = 5;

void handleAppModes() {
    // Your logic for handling the different modes goes here.
    // For instance, listening for LoRa packets, or switching modes based on button presses.
}

void updMode() {
    // Cycle through operation modes
    if (current_mode == PTT) {
        current_mode = TXT;
        updDisp(1, "Mode: TXT");
    } else if (current_mode == TXT) {
        current_mode = TST;
        updDisp(1, "Mode: TST");
    } else if (current_mode == TST) {
        current_mode = RAW;
        updDisp(1, "Mode: RAW");
    } else if (current_mode == RAW) {
        current_mode = PTT;
        updDisp(1, "Mode: PTT");
    }
    updModeAndChannelDisplay();
}

void updChannel() {
    // Cycle through channels
    channel_idx = (channel_idx + 1) % 26;  // Assuming 26 channels A-Z
    updModeAndChannelDisplay();
}