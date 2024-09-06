#include <AceButton.h>
#include <codec2.h>
#include "utilities.h"

#include "display.h"
#include "lora.h"
#include "audio.h"
#include "settings.h"
#include "app_modes.h"

using namespace ace_button;

// Define an array of mode names as strings
const char* modes[] = { "RAW","PTT", "TXT", "TST"};
const int numModes = sizeof(modes) / sizeof(modes[0]);
int modeIndex = 0;
const char* current_mode=modes[modeIndex];

// Buffer sizes and other constants
#define RAW_SIZE 160  // Adjust as necessary
#define MAX_PKT 255   // Maximum packet size

// Buffers
short raw_buf[RAW_SIZE];
unsigned char pkt_buf[MAX_PKT];

// Codec2 instance
CODEC2* codec;

// Test message counter
int test_message_counter = 0;

// Button objects
// Define the pin numbers
#define MODE_PIN _PINNUM(1,10)  // Button 1 connected to P1.10
#define TOUCH_PIN _PINNUM(0,11)  // Button 2 connected to P0.11 (Touch-capable pin)
AceButton modeButton(MODE_PIN);
AceButton touchButton(TOUCH_PIN);

void setupAppModes() {
    // Initialize buttons
    ButtonConfig* config = ButtonConfig::getSystemButtonConfig();
    config->setEventHandler(handleEvent);
    config->setLongPressDelay(1000);  // Set long press delay to 1 second
    config->setFeature(ButtonConfig::kFeatureLongPress);  // Enable long press detection
    config->setClickDelay(300);  

    modeButton.init(MODE_PIN);
    touchButton.init(TOUCH_PIN);
}

void handleEvent(ace_button::AceButton* button, uint8_t eventType, uint8_t buttonState) {
    Serial.println("Button pressed");
    Serial.println(eventType);
    Serial.println(button->getPin());

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
    } else if (button->getPin() == TOUCH_PIN && in_settings_mode) {
        if (eventType == AceButton::kEventPressed) {
            // Increment the current setting
            updateCurrentSetting();
        }
    }
}

void handleAppModes() {
    modeButton.check();
    touchButton.check();

    /*

    if (current_mode == "PTT") {
        sendAudio();
    } else if (current_mode == "TST") {
        sendTestMessage();
    } else if (current_mode == "TXT" || current_mode == "RAW") {
        handlePacket();
    }
    */
}

void sendAudio() {
    codec = codec2_create(bitrate_modes[bitrate_idx]);
    int bits_per_frame = codec2_bits_per_frame(codec);
    int enc_size = (bits_per_frame + 7) / 8;
    while (digitalRead(TOUCH_PIN) == LOW) {
        capAudio(raw_buf, RAW_SIZE);
        codec2_encode(codec, pkt_buf, raw_buf);  

        memmove(pkt_buf + 4, pkt_buf, enc_size); //Move the binary data 4 bytes, so the header can be added
        pkt_buf[0] = 'P';
        pkt_buf[1] = 'T';
        pkt_buf[2] = channels[channel_idx];  // Kanaal toevoegen
        pkt_buf[3] = bitrate_idx;  // Bitrate index toevoegen als byte
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

        if (current_mode == "RAW") {
            // Display raw message in the message buffer
            updDisp(7, (char*)pkt_buf);
        } else if (current_mode == "PTT" && strncmp((char*)pkt_buf, expected_ptt_header, 3) == 0) {
            uint8_t rcv_mode = pkt_buf[3];
            if (rcv_mode < num_bitrate_modes / sizeof(bitrate_modes[0])) {
                codec = codec2_create(bitrate_modes[rcv_mode]);
                codec2_decode(codec, raw_buf, pkt_buf + 4);
                playAudio(raw_buf, RAW_SIZE);
                updDisp(1, "Receiving...");
            } else {
                updDisp(2, "Invalid mode received");
            }
        } else if (current_mode == "TXT" && strncmp((char*)pkt_buf, expected_txt_header, 3) == 0) {
            // Display text message in the message buffer
            updDisp(7, (char*)pkt_buf);
        }
    }
}

// Function to cycle through modes
void updMode() {
    // Increment the mode index and wrap around if necessary
    modeIndex = (modeIndex + 1) % numModes;
    current_mode=modes[modeIndex];
    // Update the mode and channel display
    updModeAndChannelDisplay();
}

void updChannel() {
    // Cycle through channels using the global channel_idx from settings.h
    channel_idx = (channel_idx + 1) % 26;  // Assuming 26 channels A-Z
    updModeAndChannelDisplay();
}
