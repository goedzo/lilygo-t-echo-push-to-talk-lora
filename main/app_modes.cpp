#include <AceButton.h>
#include <codec2.h>
#include <TinyGPS++.h>
#include "utilities.h"

#include "gps.h"
#include "display.h"
#include "lora.h"
#include "audio.h"
#include "settings.h"
#include "app_modes.h"
#include "packet.h"

using namespace ace_button;

// Define an array of mode names as strings
const char* modes[] = { "RAW","TXT", "RANGE", "PONG","TST","PTT"};
const int numModes = sizeof(modes) / sizeof(modes[0]);
int modeIndex = 0;
const char* current_mode=modes[modeIndex];

uint32_t        sendTestMessageTimer = 0;
int pckt_count=0;

uint32_t        actionButtonTimer = 0;
bool ignore_next_button_press=false;

// Buffers
short raw_buf[RAW_SIZE];
//This is used for sending LORA messages
//This buffer receives LORA messages
//unsigned char rcv_pkt_buf[MAX_PKT];

// Codec2 instance
CODEC2* codec;

// Test message counter
int test_message_counter = 0;
int rcv_test_message_counter=0;

// Range Test vars
bool range_role_sender=false;
//Test counters start at 1
int range_last_count=1;
int range_consecutive_ok=0;
//We start at Null Island https://en.wikipedia.org/wiki/Null_Island
double range_home_lat=0;
double range_home_long=0;
double range_max_dist;
double range_stable_dist;
int range_total_pckt_loss=0;


// Button objects
// Define the pin numbers
#define MODE_PIN _PINNUM(1,10)  // Button 1 connected to P1.10
#define TOUCH_PIN _PINNUM(0,11)  // Button 2 connected to P0.11 (Touch-capable pin)
AceButton modeButton(MODE_PIN);
AceButton touchButton(TOUCH_PIN);

// Variables for debouncing the touch button
unsigned long lastTouchPressTime = 0; // Timestamp of the last button press
unsigned long debounceDelay = 50;     // Debounce time in milliseconds
bool touchButtonPressed = false;      // Flag to track the debounced state

// The main loop that is the logic for all app modes
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

            if(digitalRead(TOUCH_PIN) == LOW) {
              //Let's reset the counters
              test_message_counter=0;
              pckt_count=rcv_test_message_counter;
            }

            //Non blocking send
            sendTestMessage();
        } 
        else if (current_mode == "TXT") {
            if(digitalRead(TOUCH_PIN) == LOW) {
              //Let's synch the packet count to the last received test counter
              pckt_count=rcv_test_message_counter;
            }
        }
        else if (current_mode == "RAW") {
            if(digitalRead(TOUCH_PIN) == LOW) {
              //Let's synch the packet count to the last received test counter
              pckt_count=test_message_counter;
            }
        }
        else if (current_mode == "PONG") {
            if (debouncedTouchPress()) {
              //We start on the buttonpress
              updDisp(4, "Started pingpong",true);
              Serial.print(F("[SX1262] Sending another packet ... "));
              updDisp(5, "Ping!",true);
              sendPacket("Ping!");
            }
        }
        else if (current_mode == "RANGE") {
            if (debouncedTouchPress()) {
                range_role_sender=!range_role_sender;
                printRangeStatus();
            }  

            if(range_role_sender) {
                //We are a sender, so we can sent the range messages
                sendRangeMessage();
            }
            else {
                //We are receiver, will be handled with receive packet
            }


        }
    }

}

void handlePacket(Packet packet) {
    if (packet.type == "NULL") {
      // Handle unknown packet type and show the raw message
      updDisp(3, "Unknwn pcket tpe", true);

      char display_msg[30];
      snprintf(display_msg, sizeof(display_msg), "SNR: %.1f dB", radio->getSNR() );
      updDisp(5, display_msg,false);
      snprintf(display_msg, sizeof(display_msg), "RSSI: %.1f dBm", radio->getRSSI() );
      updDisp(6, display_msg,false);


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
      rawMessage[index] = '\0';  // Null-terminate the string
      updDisp(7, rawMessage, true);  // Display the raw message in hexadecimal
    } 
    else {
      // Parse the packet using the current channel configuration
      if (current_mode == "RAW" || current_mode == "TST") {
          //Cool of period to allow receiving of messages because of switching from sent to receive takes time
          sendTestMessageTimer = millis();

          pckt_count++;
          char buf[50];

          char display_msg[30];
          snprintf(display_msg, sizeof(display_msg), "SNR: %.1f dB", radio->getSNR() );
          updDisp(4, display_msg,false);
          snprintf(display_msg, sizeof(display_msg), "RSSI: %.1f dBm", radio->getRSSI() );
          updDisp(5, display_msg,false);

          snprintf(buf, sizeof(buf), "Rcv Cnt: %d", pckt_count);
          updDisp(6, buf, false);

          if (packet.isTestMessage()) {
              rcv_test_message_counter=packet.testCounter;
              snprintf(buf, sizeof(buf), "Test Cnt: %d", packet.testCounter);
              updDisp(7, buf, false);

          } else {
              updDisp(7, "", false);
          }

          updDisp(8, packet.content.c_str(), true);

      } 
      else if (current_mode == "PTT" && packet.type == "PTT") {
          if(packet.channel== channels[deviceSettings.channel_idx]) {
              //This is actually meant for my channel
              uint8_t rcv_mode = packet.content[0];
              if (rcv_mode < num_bitrate_modes / sizeof(bitrate_modes[0])) {
                  codec = codec2_create(bitrate_modes[rcv_mode]);
                  //codec2_decode(codec, raw_buf, packet.content + 1);
                  playAudio(raw_buf, RAW_SIZE);
                  updDisp(1, "Receiving...", false);
              } else {
                  updDisp(2, "Invalid mode received", true);
              }
          }
      } 
      else if (current_mode == "TXT" && packet.type == "TXT") {
          if(packet.channel== channels[deviceSettings.channel_idx]) {
              //This is actually meant for my channel
              updDisp(7, packet.content.c_str(), true);
          }
      }
      else if (current_mode == "PONG" && packet.type == "PING") {
          //We pong this message
            updDisp(5, "       pong",false);

            // Print RSSI (Received Signal Strength Indicator)
            Serial.print(F("[SX1262] RSSI:\t\t"));
            Serial.print(radio->getRSSI());
            Serial.println(F(" dBm"));

            // Print SNR (Signal-to-Noise Ratio)
            Serial.print(F("[SX1262] SNR:\t\t"));
            Serial.print(radio->getSNR());
            Serial.println(F(" dB"));

            char display_msg[30];
            snprintf(display_msg, sizeof(display_msg), "SNR: %.1f dB", radio->getSNR() );
            updDisp(6, display_msg,false);
            snprintf(display_msg, sizeof(display_msg), "RSSI: %.1f dBm", radio->getRSSI() );
            updDisp(7, display_msg,true);          

            // Send another packet
            Serial.print(F("[SX1262] Sending another packet ... "));
            updDisp(5, "Ping!",true);
            sendPacket("Ping!");

      } 
      else if (current_mode == "RANGE" && packet.type == "RANGE") {
          if(packet.channel== channels[deviceSettings.channel_idx]) {
              //Let's work on the range
              if (packet.isRangeMessage()){
                  if(packet.testCounter==range_last_count+1) {
                      //No Packet missed!
                      range_last_count++;
                      range_consecutive_ok++;
                  }
                  else {
                      //Packet loss!
                      int pckt_missed=packet.testCounter-range_last_count+1;
                      range_total_pckt_loss+=pckt_missed;
                      range_consecutive_ok=0;
                      range_last_count=packet.testCounter;
                  }

                  if(gps_status!=GPS_LOC) {
                      updDisp(4, "Wait on GPS Lock.",false);
                  }
                  else {
                      if(range_home_lat==0 && range_home_long==0) {
                        //Let's set our homebase
                        range_home_lat=gps_latitude;
                        range_home_long=gps_longitude;
                        updDisp(4, "Home Location OK",true);
                      }

                      //Let's calculate the range
                      double distance = TinyGPSPlus::distanceBetween(range_home_lat, range_home_long, gps_latitude, gps_longitude);
                      if(range_max_dist < distance) {
                          range_max_dist = distance;
                      }
                      if(range_consecutive_ok>2) {
                          //We had no packet loss, so let's make it the stable distance
                          range_stable_dist=distance;
                      }

                      //Let's print the info

                      updDisp(4, packet.content.c_str(),false); //test message and counter
                      char display_msg[30];
                      snprintf(display_msg, sizeof(display_msg), "Dist: %dm Max: %dm", range_max_dist);
                      updDisp(5, display_msg,false); //test message and counter
                      snprintf(display_msg, sizeof(display_msg), "Stable: %dm", range_stable_dist);
                      updDisp(6, display_msg,false); //test message and counter
                      snprintf(display_msg, sizeof(display_msg), "Pckt Loss: %dm", range_total_pckt_loss);
                      updDisp(7, display_msg,false); //test message and counter
                      snprintf(display_msg, sizeof(display_msg), "Pckt Ok: %dm", range_consecutive_ok);
                      updDisp(8, display_msg,false); //test message and counter
                  }
              }
              else {
                  showError("no range pckt");
              }
          }
      }
    }
}


void printRangeStatus() {
    if(range_role_sender) {
        updDisp(2, "Role: Sender",true);
    }
    else {
        updDisp(2, "Role: Receiver",false);
        if(gps_status==NO_GPS) {
            showError("No GPS Installed!");
        }
        else if (gps_status==GPS_LOC) {
            //Location ok!
            updDisp(4, "",true);
        } else {
            updDisp(4, "Wait on GPS Lock.",true);
        }
        updDisp(5, "Wait for pckg",true);
    }
}

// Function to handle debouncing for the touch button
bool debouncedTouchPress() {
    bool currentState = (digitalRead(TOUCH_PIN) == LOW);
    unsigned long currentTime = millis();

    if (currentState != touchButtonPressed) {
        // Button state has changed, reset the debounce timer
        lastTouchPressTime = currentTime;
        touchButtonPressed = currentState;
    }

    // If the button is pressed and the debounce delay has passed, return true
    if (currentState && (currentTime - lastTouchPressTime > debounceDelay)) {
        return true;
    }

    return false;
}

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

void sendAudio() {
    codec = codec2_create(bitrate_modes[deviceSettings.bitrate_idx]);
    int bits_per_frame = codec2_bits_per_frame(codec);
    int enc_size = (bits_per_frame + 7) / 8;
    while (digitalRead(TOUCH_PIN) == LOW) {

        unsigned char send_pkt_buf[MAX_PKT];
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

void sendRangeMessage() {
    if (millis() - sendTestMessageTimer > 5000 ) {
      sendTestMessageTimer = millis();

      test_message_counter++;
      char test_msg[50];
      snprintf(test_msg, sizeof(test_msg), "test%d", test_message_counter);

      char send_pkt_buf[50];
      snprintf((char*)send_pkt_buf, sizeof(send_pkt_buf), "RN%c%s%d", channels[deviceSettings.channel_idx], "test", test_message_counter);

      char display_msg[30];
      snprintf(display_msg, sizeof(display_msg), "Sent: %s", test_msg);
      updDisp(4, display_msg,true);

      sendPacket(send_pkt_buf);
      //Give the device some small time to process sending
      delay(100);
      snprintf(display_msg, sizeof(display_msg), "TmOnAr: %d", timeOnAir);
      updDisp(5, display_msg,true);

      //Cool of period to allow receiving of messages because of switching from sent to receive takes time
      sendTestMessageTimer = millis();
    }

}


void sendTestMessage(bool now) {
    //Only do this every 2 seconds or when now=true

    if (millis() - sendTestMessageTimer > 5000 || now) {
      sendTestMessageTimer = millis();

      test_message_counter++;
      char test_msg[50];
      snprintf(test_msg, sizeof(test_msg), "test%d", test_message_counter);

      char send_pkt_buf[50];
      snprintf((char*)send_pkt_buf, sizeof(send_pkt_buf), "TX%c%s%d", channels[deviceSettings.channel_idx], "test", test_message_counter);

      char display_msg[30];
      snprintf(display_msg, sizeof(display_msg), "Sent: %s", test_msg);
      updDisp(2, display_msg,true);

      sendPacket(send_pkt_buf);
      //Give the device some small time to process sending
      delay(100);
      snprintf(display_msg, sizeof(display_msg), "TmOnAr: %d", timeOnAir);
      updDisp(3, display_msg,true);

      //Cool of period to allow receiving of messages because of switching from sent to receive takes time
      sendTestMessageTimer = millis();
    }

}

// Function to cycle through modes
void updMode() {
    // Increment the mode index and wrap around if necessary
    modeIndex = (modeIndex + 1) % numModes;
    current_mode=modes[modeIndex];

    if(current_mode=="TST" || current_mode=="RAW" || current_mode=="TXT" || current_mode=="PONG" || current_mode=="RANGE" ) {
        //We need to reinit the right radio
        setupLoRa();
    }


    // Update the mode and channel display
    clearScreen();
    updModeAndChannelDisplay();
    if(current_mode=="RANGE") {
        printRangeStatus();
    }


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