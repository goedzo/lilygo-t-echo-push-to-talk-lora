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

uint32_t        appmodeTimer = 0;
int pckt_count=0;


// Buffer sizes and other constants
#define RAW_SIZE 160  // Adjust as necessary
#define MAX_PKT 255   // Maximum packet size

// Buffers
short raw_buf[RAW_SIZE];
//This is used for sending LORA messages
unsigned char send_pkt_buf[MAX_PKT];
//This buffer receives LORA messages
unsigned char rcv_pkt_buf[MAX_PKT];

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
    config->setLongPressDelay(1500);  // Set long press delay to 1 second
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
    } else if (button->getPin() == TOUCH_PIN && !in_settings_mode) {
        if (eventType == AceButton::kEventPressed) {
            // We pressed the touch pin while not in settings
        }
    }
}

void handleAppModes() {
    modeButton.check();
    touchButton.check();

    if (!in_settings_mode) {
        if (current_mode == "PTT") {
            //sendAudio();
        } else if (current_mode == "TST") {
            sendTestMessage();
            //Allow receiving of messages
            handlePacket();
        } else if (current_mode == "TXT" || current_mode == "RAW") {
            handlePacket();
        }
    }

}

void sendAudio() {
    codec = codec2_create(bitrate_modes[deviceSettings.bitrate_idx]);
    int bits_per_frame = codec2_bits_per_frame(codec);
    int enc_size = (bits_per_frame + 7) / 8;
    while (digitalRead(TOUCH_PIN) == LOW) {
        capAudio(raw_buf, RAW_SIZE);
        codec2_encode(codec, send_pkt_buf, raw_buf);  

        memmove(send_pkt_buf + 4, send_pkt_buf, enc_size); //Move the binary data 4 bytes, so the header can be added
        send_pkt_buf[0] = 'P';
        send_pkt_buf[1] = 'T';
        send_pkt_buf[2] = channels[deviceSettings.channel_idx];  // Kanaal toevoegen
        send_pkt_buf[3] = deviceSettings.bitrate_idx;  // Bitrate index toevoegen als byte
        sendPacket(send_pkt_buf, enc_size + 4);
        updDisp(1, "Transmitting...");
        handlePacket();  // Allow receiving packets during transmission

        //Allow buttonpresses
        modeButton.check();
        touchButton.check();


    }
}

void sendTestMessage() {
    //Only do this every 1 seconds

    if(digitalRead(TOUCH_PIN) == LOW) {
      //Let's reset the counters
      test_message_counter=0;
    }

    if (millis() - appmodeTimer > 2000) {
      appmodeTimer = millis();

      char test_msg[20];
      snprintf(test_msg, sizeof(test_msg), "test%d", ++test_message_counter);

      snprintf((char*)send_pkt_buf, 4, "TX%c", channels[deviceSettings.channel_idx]);
      strcpy((char*)send_pkt_buf + 3, test_msg);
      sendPacket(send_pkt_buf, strlen(test_msg) + 3);

      char display_msg[30];
      snprintf(display_msg, sizeof(display_msg), "Sent: %s", test_msg);
      updDisp(4, display_msg);

    }

}


void handlePacket() {
    int pkt_size = receivePacket(rcv_pkt_buf, MAX_PKT);
    if (pkt_size) {
        rcv_pkt_buf[pkt_size] = '\0';  // Null-terminate the received packet

        char expected_ptt_header[4];
        snprintf(expected_ptt_header, sizeof(expected_ptt_header), "PT%c", channels[deviceSettings.channel_idx]);

        char expected_txt_header[4];
        snprintf(expected_txt_header, sizeof(expected_txt_header), "TX%c", channels[deviceSettings.channel_idx]);

        if (current_mode == "RAW" || current_mode == "TST") {
            // Display raw message and increment packet counter
            pckt_count++;
            char buf[50];
            snprintf(buf, sizeof(buf), "Pckt Cnt: %d", pckt_count);
            updDisp(5, buf, false);
            snprintf(buf, sizeof(buf), "Pckt Len: %d", pkt_size);
            updDisp(6, buf, false);
            updDisp(7, (char*)rcv_pkt_buf, true);

            // Extract the header (first 3 characters)
            char header[4];
            strncpy(header, (char*)rcv_pkt_buf, 3);
            header[3] = '\0';  // Null-terminate the header

            // Extract the contents (everything after the first 3 characters)
            char* contents = (char*)rcv_pkt_buf + 3;

            // Check if the message is "TXA", "TXB", etc. and contains a valid test message
            if (strncmp(header, "TX", 2) == 0) {
                // Assume the format "test<number>" after the header
                if (strncmp(contents, "test", 4) == 0) {
                    char* test_counter_str = contents + 4;  // Pointer to the part after "test"

                    // Try to extract a numeric test counter
                    int test_counter = atoi(test_counter_str);
                    if (test_counter > 0) {

                        if (current_mode == "RAW" )
                            if (digitalRead(TOUCH_PIN) == LOW) {
                                // Synch the packet counter
                                pckt_count = test_counter;
                            }
                        }

                        snprintf(buf, sizeof(buf), "TST Count: %d", test_counter);
                        updDisp(8, buf, true);  // Display the counter
                    } else {
                        updDisp(8, "Invalid Test Counter", true);
                    }
                } else {
                    updDisp(8, "Invalid Test Message", true);
                }
            }

        } else if (current_mode == "PTT" && strncmp((char*)rcv_pkt_buf, expected_ptt_header, 3) == 0) {
            uint8_t rcv_mode = rcv_pkt_buf[3];
            if (rcv_mode < num_bitrate_modes / sizeof(bitrate_modes[0])) {
                codec = codec2_create(bitrate_modes[rcv_mode]);
                codec2_decode(codec, raw_buf, rcv_pkt_buf + 4);
                playAudio(raw_buf, RAW_SIZE);
                updDisp(1, "Receiving...", false);
            } else {
                updDisp(2, "Invalid mode received", true);
            }
        } else if (current_mode == "TXT" && strncmp((char*)rcv_pkt_buf, expected_txt_header, 3) == 0) {
            // Display text message in the message buffer
            updDisp(7, (char*)rcv_pkt_buf, true);
        }
    }
}


void handlePacket_old() {
    int pkt_size = receivePacket(rcv_pkt_buf, MAX_PKT);
    if (pkt_size) {
        rcv_pkt_buf[pkt_size] = '\0';

        char expected_ptt_header[4];
        snprintf(expected_ptt_header, sizeof(expected_ptt_header), "PT%c", channels[deviceSettings.channel_idx]);

        char expected_txt_header[4];
        snprintf(expected_txt_header, sizeof(expected_txt_header), "TX%c", channels[deviceSettings.channel_idx]);

        if (current_mode == "RAW" || current_mode == "TST") {
            if(digitalRead(TOUCH_PIN) == LOW) {
              //Let's reset the counters
              pckt_count=0;
            }

            // Display raw message in the message buffer
            pckt_count++;
            char buf[50];
            snprintf(buf, sizeof(buf), "Pckt Cnt: %d", pckt_count);
            updDisp(5, buf,false);
            snprintf(buf, sizeof(buf), "Pckt Len: %d", pkt_size);
            updDisp(6, buf,false);
            updDisp(7, (char*)rcv_pkt_buf,true);
        } else if (current_mode == "PTT" && strncmp((char*)rcv_pkt_buf, expected_ptt_header, 3) == 0) {
            uint8_t rcv_mode = rcv_pkt_buf[3];
            if (rcv_mode < num_bitrate_modes / sizeof(bitrate_modes[0])) {
                codec = codec2_create(bitrate_modes[rcv_mode]);
                codec2_decode(codec, raw_buf, rcv_pkt_buf + 4);
                playAudio(raw_buf, RAW_SIZE);
                updDisp(1, "Receiving...",false);
            } else {
                updDisp(2, "Invalid mode received",true);
            }
        } else if (current_mode == "TXT" && strncmp((char*)rcv_pkt_buf, expected_txt_header, 3) == 0) {
            // Display text message in the message buffer
            updDisp(7, (char*)rcv_pkt_buf,true);
        }
    }
}

// Function to cycle through modes
void updMode() {
    // Increment the mode index and wrap around if necessary
    modeIndex = (modeIndex + 1) % numModes;
    current_mode=modes[modeIndex];
    // Update the mode and channel display
    clearScreen();
    updModeAndChannelDisplay();
    //Make sure we keep receiving messages
    setupLoRa();

}

void updChannel() {
    updModeAndChannelDisplay();
}
