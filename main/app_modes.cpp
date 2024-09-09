#include <AceButton.h>
#include <codec2.h>
#include "utilities.h"

#include "gps.h"
#include "display.h"
#include "lora.h"
#include "audio.h"
#include "settings.h"
#include "app_modes.h"
#include "packet.h"
#include "pingpong.h"

using namespace ace_button;

// Define an array of mode names as strings
const char* modes[] = { "PONG","RAW","PTT", "TXT", "TST"};
const int numModes = sizeof(modes) / sizeof(modes[0]);
int modeIndex = 0;
const char* current_mode=modes[modeIndex];

uint32_t        appmodeTimer = 0;
int pckt_count=0;

uint32_t        actionButtonTimer = 0;
bool ignore_next_button_press=false;

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
    config->setFeature(ButtonConfig::kFeatureLongPress);  // Enable long press detection
    config->setFeature(ButtonConfig::kFeatureDoubleClick); // Enable double click detection
    config->setFeature(ButtonConfig::kFeatureSuppressAfterClick);
    config->setFeature(ButtonConfig::kFeatureSuppressAfterDoubleClick);

    config->setClickDelay(125);  

    /*
    config->setLongPressDelay(1000);  // Set long press delay to 1 second
    config->setDoubleClickDelay(300);
    */

    modeButton.init(MODE_PIN);
    touchButton.init(TOUCH_PIN);
}

void handleEvent(ace_button::AceButton* button, uint8_t eventType, uint8_t buttonState) {
    //Serial.println("Button pressed");
    //Serial.println(eventType);
    //Serial.println(button->getPin());

    if (button->getPin() == MODE_PIN) {
        if (eventType == AceButton::kEventReleased) {
            if(ignore_next_button_press) {
                ignore_next_button_press=false;
                return;
            }
            /** This version uses a Released event instead of a Clicked,
            * while suppressing the Released after a DoubleClicked, and we ignore the
            * Clicked event that we can't suppress. The disadvantage of this version is
            * that if a user accidentally makes a normal Clicked event (a rapid
            * Pressed/Released), nothing happens. Depending on the application,
            * this may or may not be the desirable result.          */
            if (in_settings_mode) {
                // Cycle through different settings when in settings mode
                cycleSettings();
            } else {
                // Single click cycles through modes
                updMode();
            }
        }
        else if (eventType == AceButton::kEventDoubleClicked) {
            // Double click, so quickly increase the SPF
            deviceSettings.nextSpreadingFactor();
            //Reset lora to use the new SPF
            setupLoRa();
            updModeAndChannelDisplay();
            return;
        } 
        else if (eventType == AceButton::kEventLongPressed) {
            // Long press enters or exits settings mode
            ignore_next_button_press=true;
            toggleSettingsMode();
            return;
        } 
        
    } else if (button->getPin() == TOUCH_PIN && in_settings_mode) {
        if (eventType == AceButton::kEventPressed) {
            // Increment the current setting
            updateCurrentSetting();
        }
    } else if (button->getPin() == TOUCH_PIN && !in_settings_mode) {
        if (eventType == AceButton::kEventPressed) {
            // We pressed the touch pin while not in settings
            // Don't implement anything here, but use digitalRead in handleAppModes() 
            //for better reading and better dealing with button handling performance
        }
    }
}

void powerOff() {
    // Power Off display message
    //Note that OFF is not part of the mode ENUM, to avoid it going there when switching modes
    current_mode="OFF";
    clearScreen();
    drawIcon(off_icon,0, disp_top_margin,16, 16, GxEPD_WHITE, GxEPD_BLACK);
    updDisp(4, "Powered Off..", false);
    updDisp(7, "Press reset button", false);
    updDisp(8, "to turn on.", true);

    //Make sure we turn of the backlight
    enableBacklight(false);

    // Step 1: Shut down peripherals
    sleepLoRa();       // Put LoRa module to sleep
    sleepAudio();      // Put audio subsystem to sleep (implement sleepAudio() for your specific setup)
    turnoffLed();      // Turn off the LED
    //sleepGPS();        // Put GPS to sleep if available
    // Add other peripheral shutdowns as needed

    // Step 2: Disable unnecessary GPIO pins
    pinMode(MODE_PIN, INPUT);       // Set button pin to low power state
    pinMode(TOUCH_PIN, INPUT);      // Set touch pin to low power state
    // Set other pins to INPUT if needed to prevent power leakage


    // Step 3: Shut down serial interfaces to save power
    Serial.end();

    // Step 4: Enter deep sleep mode (System OFF mode on nRF52840)
    uint8_t sd_en;
    (void)sd_softdevice_is_enabled(&sd_en);

    if (sd_en) {
        sd_power_system_off();  // Use softdevice to turn off the system
    } else {
        NRF_POWER->SYSTEMOFF = 1;  // Put nRF52840 into System OFF mode
    }

    // Note: The system will remain in this state until reset or a wake-up interrupt occurs.
}

void handleAppModes() {
    modeButton.check();
    touchButton.check();

    //Always allow receiving and sending messages;
    checkLoraPacketComplete(); //If a message was received, it will call handlePacket();

    //Update GPS location
    loopGPS();

    //Let's implement a power off, when the action button is pressed 5 seconds
    if(digitalRead(MODE_PIN) == LOW) {
      //Keep counting until 5 seconds
      if (millis() - actionButtonTimer > 5000) {
        powerOff();
      }

    }
    else if (digitalRead(MODE_PIN) == HIGH) {
      //No button pressed
      actionButtonTimer=millis();
    }


    if (!in_settings_mode) {
        if (current_mode == "PTT") {
            //sendAudio();
        } else if (current_mode == "TST") {
            //Non blocking send
            sendTestMessage();
        } else if (current_mode == "TXT" || current_mode == "RAW") {
            if(digitalRead(TOUCH_PIN) == LOW) {
              //Let's synch the packet count to the last received test counter
              pckt_count=test_message_counter;
            }
        }
        else if (current_mode == "PONG") {
            pingpongLoop();
            if(digitalRead(TOUCH_PIN) == LOW) {
              //We start on the buttonpress
              updDisp(4, "Started pingpong",true);
              pingpongStart();
            }
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
        checkLoraPacketComplete(); // Allow receiving packets during transmission

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

      char test_msg[50];
      snprintf(test_msg, sizeof(test_msg), "test%d", ++test_message_counter);

      snprintf((char*)send_pkt_buf, 4, "TX%c", channels[deviceSettings.channel_idx]);
      strcpy((char*)send_pkt_buf + 3, test_msg);


      char display_msg[30];
      snprintf(display_msg, sizeof(display_msg), "Sent: %s", test_msg);
      updDisp(4, display_msg);


      sendPacket(send_pkt_buf, strlen(test_msg)+3);


    }

}

void handlePacket() {
    int pkt_size = receivePacket(rcv_pkt_buf, MAX_PKT);
    if (pkt_size) {
        rcv_pkt_buf[pkt_size] = '\0';  // Null-terminate the received packet

        Packet packet;

        // Parse the packet using the current channel configuration
        if (packet.parsePacket(rcv_pkt_buf, pkt_size)) {
            if (current_mode == "RAW" || current_mode == "TST") {
                pckt_count++;
                char buf[50];

                char display_msg[30];
                snprintf(display_msg, sizeof(display_msg), "SNR: %.3f  dB", radio->getSNR() );
                updDisp(3, display_msg,false);
                snprintf(display_msg, sizeof(display_msg), "RSSI: %.3f dBm", radio->getRSSI() );
                updDisp(4, display_msg,false);

                snprintf(buf, sizeof(buf), "Rcv Cnt: %d", pckt_count);
                updDisp(5, buf, false);

                if (packet.isTestMessage()) {
                    test_message_counter=packet.testCounter;
                    snprintf(buf, sizeof(buf), "Test Cnt: %d", packet.testCounter);
                    updDisp(6, buf, false);
                } else {
                    updDisp(6, "", false);
                }

                updDisp(7, packet.content.c_str(), true);

            } else if (current_mode == "PTT" && packet.type == "PTT") {
                if(packet.channel== channels[deviceSettings.channel_idx]) {
                    //This is actually meant for my channel
                    uint8_t rcv_mode = rcv_pkt_buf[3];
                    if (rcv_mode < num_bitrate_modes / sizeof(bitrate_modes[0])) {
                        codec = codec2_create(bitrate_modes[rcv_mode]);
                        codec2_decode(codec, raw_buf, rcv_pkt_buf + 4);
                        playAudio(raw_buf, RAW_SIZE);
                        updDisp(1, "Receiving...", false);
                    } else {
                        updDisp(2, "Invalid mode received", true);
                    }
                }
            } else if (current_mode == "TXT" && packet.type == "TXT") {
                if(packet.channel== channels[deviceSettings.channel_idx]) {
                    //This is actually meant for my channel
                    updDisp(7, packet.content.c_str(), true);
                }

            }
        } else if (packet.type == "NULL") {
            // Handle unknown packet type and show the raw message
            updDisp(3, "Unknwn pcket tpe", true);

            char display_msg[30];
            snprintf(display_msg, sizeof(display_msg), "SNR: %.3f  dB", radio->getSNR() );
            updDisp(4, display_msg,false);
            snprintf(display_msg, sizeof(display_msg), "RSSI: %.3f dBm", radio->getRSSI() );
            updDisp(5, display_msg,false);


            char rawMessage[packet.rawLength * 4 + 1];  // Enough space for each byte as either ASCII or hex
            uint16_t index = 0;
            for (uint16_t i = 0; i < packet.rawLength; i++) {
                if (isprint(packet.raw[i])) {
                    rawMessage[index++] = packet.raw[i];  // Copy printable characters directly
                } else {
                    sprintf(rawMessage + index, "\\x%02X", packet.raw[i]);  // Convert non-printable byte to hex
                    index += 4;  // Move index forward by 4 (for \xNN)
                }
            }
            updDisp(7, rawMessage, true);  // Display the raw message in hexadecimal
        }
    }
}



// Function to cycle through modes
void updMode() {
    // Increment the mode index and wrap around if necessary
    modeIndex = (modeIndex + 1) % numModes;
    current_mode=modes[modeIndex];

    if(current_mode=="TST" || current_mode=="RAW" || current_mode=="TXT" ) {
        //We need to reinit the radio
        setupLoRa();
    }

    if(current_mode=="PONG") {
        //We need to reinit the radio
        setupPingPong();
    }

    // Update the mode and channel display
    clearScreen();
    updModeAndChannelDisplay();
    //Make sure we keep receiving messages
    //setupLoRa();

}

void updChannel() {
    updModeAndChannelDisplay();
}

void sleepAudio() {
    // If using Codec2, disable or reset the codec instance
    if (codec) {
        codec2_destroy(codec);  // Release Codec2 resources
        Serial.println(F("Audio codec is now disabled."));
    }

    // If using any DAC or external audio hardware, handle those here
    // Example: pinMode(DAC_PIN, INPUT);  // Set DAC pin to a low-power state
}

void turnoffLed(){
    digitalWrite(GreenLed_Pin, HIGH);
    digitalWrite(RedLed_Pin, HIGH);
    digitalWrite(BlueLed_Pin, HIGH);
}