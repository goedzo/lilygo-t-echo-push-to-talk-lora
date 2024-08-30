#include "app_modes.h"
#include "display.h"
#include "lora.h"
#include "audio.h"
#include "settings.h"
#include <AceButton.h>

using namespace ace_button;

OperationMode current_mode = PTT;

// Buffer sizes and other constants
#define RAW_SIZE 160  // Adjust as necessary
#define MAX_PKT 255   // Maximum packet size

// Buffers
int16_t raw_buf[RAW_SIZE];
uint8_t pkt_buf[MAX_PKT];

// Codec2 instance
void* codec;

// Test message counter
int test_message_counter = 0;

// Message display buffer
char message_lines[10][30];
int current_message_line = 0;

// Button objects
AceButton modeButton(MODE_PIN);
AceButton actionButton(ACTION_PIN);

void setupAppModes() {
    // Initialize buttons
    ButtonConfig* config = ButtonConfig::getSystemButtonConfig();
    config->setEventHandler(handleEvent);

    modeButton.init(MODE_PIN);
    actionButton.init(ACTION_PIN);
}

void handleEvent(AceButton* button, uint8_t eventType, uint8_t buttonState) {
    if (button->getPin() == MODE_PIN) {
        if (eventType == AceButton::kEventLongPressed) {
            // Long press enters or exits settings mode
            toggleSettingsMode();
        } else if (eventType == AceButton::kEventPressed) {
            if (in_settings_mode) {
                // Cycle through different settings when in settings mode
                cycleSettings();
            } else {
                // Single click cycles through modes
                updMode();
            }
        }
    } else if (button->getPin() == ACTION_PIN && in_settings_mode) {
        if (eventType == AceButton::kEventLongPressed && setting_idx == 3) {
            // Switch between hours, minutes, and seconds in time setting
            cycleTimeSettingMode();
        } else if (eventType == AceButton::kEventPressed) {
            // Increment the current setting
            updateCurrentSetting();
        }
    }
}

void handleAppModes() {
    if (current_mode == PTT) {
        sendAudio();
    } else if (current_mode == TST) {
        sendTestMessage();
    } else if (current_mode == TXT || current_mode == RAW) {
        handlePacket();
    }
}

void sendAudio() {
    while (digitalRead(ACTION_PIN) == LOW) {
        capAudio(raw_buf, RAW_SIZE);
        uint16_t enc_size = codec2_encode(codec, pkt_buf + 4, raw_buf);  

        snprintf((char*)pkt_buf, 4, "PT%c", channels[channel_idx]);
        sendPacket(pkt_buf, enc_size + 4);
        updDisp(1, "Transmitting...");

        handlePacket();  // Allow receiving packets during transmission
    }
}

void sendTestMessage() {
    char test_msg[20];
    snprintf(test_msg, sizeof(test_msg), "test%d", ++test_message_counter);

    snprintf((char*)pkt_buf, 4, "TX%c", channels[channel_idx]);
    strcpy((char*)pkt_buf + 3, test_msg);
    sendPacket(pkt_buf, strlen(test_msg) + 3);

    char display_msg[30];
    snprintf(display_msg, sizeof(display_msg), "Sent: %s", test_msg);
    updDisp(1, display_msg);
}

void handlePacket() {
    int pkt_size = receivePacket(pkt_buf, MAX_PKT);
    if (pkt_size) {
        pkt_buf[pkt_size] = '\0';

        char expected_ptt_header[4];
        snprintf(expected_ptt_header, sizeof(expected_ptt_header), "PT%c", channels[channel_idx]);

        char expected_txt_header[4];
        snprintf(expected_txt_header, sizeof(expected_txt_header), "TX%c", channels[channel_idx]);

        if (current_mode == RAW) {
            // Display raw message in the message buffer
            strncpy(message_lines[current_message_line], (char*)pkt_buf, sizeof(message_lines[current_message_line]) - 1);
            current_message_line = (current_message_line + 1) % 10;
            updateMessageDisplay();
        } else if (current_mode == PTT && strncmp((char*)pkt_buf, expected_ptt_header, 3) == 0) {
            uint8_t rcv_mode = pkt_buf[3];
            if (rcv_mode < sizeof(modes) / sizeof(modes[0])) {
                codec = codec2_create(modes[rcv_mode]);
                codec2_decode(codec, raw_buf, pkt_buf + 4);
                playAudio(raw_buf, RAW_SIZE);
                updDisp(1, "Receiving...");
            } else {
                updDisp(2, "Invalid mode received");
            }
        } else if (current_mode == TXT && strncmp((char*)pkt_buf, expected_txt_header, 3) == 0) {
            // Display text message in the message buffer
            strncpy(message_lines[current_message_line], (char*)pkt_buf + 3, sizeof(message_lines[current_message_line]) - 1);
            current_message_line = (current_message_line + 1) % 10;
            updateMessageDisplay();
        }
    }
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
    // Cycle through channels using the global channel_idx from settings.h
    channel_idx = (channel_idx + 1) % 26;  // Assuming 26 channels A-Z
    updModeAndChannelDisplay();
}

void updModeAndChannelDisplay() {
    // Update the display with the current mode and channel
    char buf[30];
    snprintf(buf, sizeof(buf), "chn:%c Bitrate: %d bps", channels[channel_idx], getBitrateFromIndex(bitrate_idx));
    updDisp(0, buf);
}