#include <AceButton.h>
#include <TinyGPS++.h>
#include "utilities.h"

#include "gps.h"
#include "display.h"
#include "lora.h"
#include "settings.h"
#include "app_modes.h"
#include "packet.h"
#include "text_inbox.h"
#include "scan.h"

using namespace ace_button;

// Define an array of mode names as strings
const char* modes[] = { "BEACON","RAW","TXT", "RANGE", "TST","PONG","SCAN","PTT"};
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




// Test message counter
int test_message_counter = 0;
int rcv_test_message_counter=0;

// TXT Multi-packet reassembly state
#define TXT_MULTI_MAX_MSG_LEN 512
#define TXT_MULTI_TIMEOUT_MS 30000
static char txt_reassemble_buf[TXT_MULTI_MAX_MSG_LEN];
static uint8_t txt_reassemble_expected, txt_reassemble_recv_count;
static uint32_t txt_reassemble_timer;

// TXT Mode Inbox display state
uint8_t  txtInboxScrollPage = 0;
uint8_t  txtInboxMsgCount = 0;
bool     txtShowInbox = false;

// Range Test vars
bool range_role_sender=false;
//Test counters start at 1
int range_last_count=0;
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


void switchMode(String receivedMode) {
    // Find the index of the received mode
    int newModeIndex = -1;
    for (int i = 0; i < numModes; i++) {
        if (receivedMode == modes[i]) {
            if(i==0) {
              newModeIndex = numModes-1;
            }
            else {
              newModeIndex = i-1;
            }
            break;
        }
    }

    // If the mode is found, update the modeIndex and current_mode
    if (newModeIndex != -1) {
        modeIndex = newModeIndex;
        current_mode = modes[modeIndex];
        updMode();  // Update mode based on the new selection
    } else {
        Serial.println("Received mode is not valid.");
    }
}

// Function to cycle through modes
void updMode() {
    // Increment the mode index and wrap around if necessary
    modeIndex = (modeIndex + 1) % numModes;
    current_mode=modes[modeIndex];

    if(current_mode=="TST" || current_mode=="RAW" || current_mode=="TXT" || current_mode=="PONG" || current_mode=="RANGE" || current_mode=="SCAN" || current_mode=="BEACON") {
        //We need to reinit the right radio
        setupLoRa();
    }


    // Update the mode and channel display
    clearScreen();
    updModeAndChannelDisplay();
    if(current_mode=="RANGE") {
        //Make sure we reset the count
        range_last_count=0;

        printRangeStatus();
    }

    // Handle mode-specific initialization
    if (current_mode == "SCAN") {
        startScanFrequencies();  // Start the scan process when SCAN mode is selected
    } else {
        stopScanFrequencies();   // Ensure the scan is stopped when switching out of SCAN mode
    }

}


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
            // PTT transmit: receiveOpusFrame() called from ble.cpp when phone sends Opus data.
            // The binary frame from BLE is forwarded to LoRa here when the phone app initiates.
        } 
        else if (current_mode == "TST") {

            if(digitalRead(TOUCH_PIN) == LOW) {
              //Let's reset the counters
              test_message_counter=0;
              pckt_count=rcv_test_message_counter;
            }

            //Non blocking send
            sendTestMessage();
        } 
        else if (current_mode == "TXT") {
            // Show latest message or inbox scroll view
            txtModeInboxDisplay();
        }

        // Mode-specific behavior for button actions
        if (!in_settings_mode && current_mode == "TXT" && !txtShowInbox) {
            // In single-message view, MODE click toggles to inbox view (only when not scrolling)
            // This is handled in handleEvent() for proper timing with debounce
        }
        else if (current_mode == "RAW") {
            if(digitalRead(TOUCH_PIN) == LOW) {
              //Let's synch the packet count to the last received test counter
              pckt_count=rcv_test_message_counter;
            }
        }
        else if (current_mode == "PONG") {
            if (debouncedTouchPress()) {
              //We start on the buttonpress
              updDisp(4, "Started pingpong",true);
              Serial.println(F("[SX1262] Sending another packet ... "));
              updDisp(5, "Ping!",true);
              sendPacket("Ping!");
            }
        }
        else if (current_mode == "RANGE") {
            if (debouncedTouchPress()) {
                range_role_sender=!range_role_sender;
                //Reset the counters
                range_last_count=0;
                range_total_pckt_loss=0;
                range_consecutive_ok=0;
                clearScreen();
                updModeAndChannelDisplay();
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
        else if (current_mode == "SCAN") {
            if(digitalRead(TOUCH_PIN) == LOW) {
                //Let's clearout the scan-list
                initTopChannels();
            }

            // Handle the SCAN mode
            handleFrequencyScan();  // Call the non-blocking scan function
        }
        
        else if (current_mode == "BEACON") {
            // Periodically broadcast our own beacon, display roster
            static unsigned long lastBeaconSend = 0;
            if (millis() - lastBeaconSend >= 23500) {  // Every ~23.5s (half of PEER_BEACON_INTERVAL)
                lastBeaconSend = millis();
                sendPeerBeacon();
                
                char buf[20];
                snprintf(buf, sizeof(buf), "Beacon: %d", peerRosterCount);
                updDisp(2, buf, true);
            }
            
            // Display roster
            beaconDisplayRoster(3);
        }    
    }
}

void handlePacket(Packet packet) {
    if (packet.type == "REQ") {  // Request for retransmission
        unsigned int requestedCounter = atoi(packet.content.c_str()); 
        handleRetransmitRequest(requestedCounter);
        return;
    }
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
      if (current_mode == "BEACON" && packet.type == "BEACON") {
          // BEACON mode: add peer to roster and display it
          beaconAddOrUpdate(packet);
          
          char buf[30];
          snprintf(buf, sizeof(buf), "%d peers", peerRosterCount);
          updDisp(2, buf, true);
          
          // Display roster entries starting at line 3
          int displayLine = 3;
          int maxLines = (disp_height + disp_top_margin) / disp_font_height;
           for (int i = 0; i < peerRosterCount && displayLine < maxLines; i++) {
               float dist = peerRoster[i].distanceM;
               const char* displayName = peerRoster[i].callSign[0] != '\0' ? peerRoster[i].callSign : peerRoster[i].deviceId.c_str();
               snprintf(buf, sizeof(buf), "%s", displayName);
              
              if (dist > 0 && dist < 1000) {
                  char distBuf[20];
                  snprintf(distBuf, sizeof(distBuf), " %.0fm", dist);
                  strncat(buf, distBuf, sizeof(buf) - strlen(buf) - 1);
              } else if (dist >= 1000) {
                  char distBuf[20];
                  snprintf(distBuf, sizeof(distBuf), " %.1fkm", dist / 1000.0);
                  strncat(buf, distBuf, sizeof(buf) - strlen(buf) - 1);
              } else if (gps_status != GPS_LOC) {
                  strncat(buf, " ???m", sizeof(buf) - strlen(buf) - 1);
              }
              
              // Battery
              int batt = peerRoster[i].battery;
              if (batt > 0) {
                  char battBuf[12];
                  snprintf(battBuf, sizeof(battBuf), " %d%%", batt);
                  strncat(buf, battBuf, sizeof(buf) - strlen(buf) - 1);
              } else {
                  strncat(buf, " -%", sizeof(buf) - strlen(buf) - 1);
              }
              
              updDisp(displayLine, buf, true);
              displayLine++;
          }
          
          // Clear remaining lines
          for (int i = displayLine; i < maxLines && i < 20; i++) {
              updDisp(i, "", false);
          }
      }
      else if (current_mode == "RAW" || current_mode == "TST") {
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
          updDisp(7, packet.content.c_str(), true);
          updModeAndChannelDisplay();
      } 
      else if (current_mode == "PTT" && packet.type == "PTT") {
          // Received PTT audio via LoRa — forward Opus bytes to connected phone via BLE
          extern void sendBinaryNotification(const uint8_t* data, uint8_t len);
          if(packet.channel== channels[deviceSettings.channel_idx]) {
              // Use raw buffer to avoid Arduino String() null-byte truncation
              uint8_t* p = packet.raw;
              uint16_t off = 0;
              
              // Skip "PT" (2) + channel (1) + 'O' (1) = 4 bytes header
              if (packet.rawLength >= 4 && p[0] == 'P' && p[1] == 'T' && p[3] == 'O') {
                  off = 4;
                  // Skip "~~" separator to get to content
                  while (off < packet.rawLength - 1 && !(p[off] == '~' && p[off+1] == '~')) off++;
                  off += 2;
              }
              
              if (off + 4 <= packet.rawLength) {
                  int opusLen = packet.rawLength - off;
                  if (opusLen > 0 && opusLen < 120) {
                      uint8_t frame[124];
                      frame[0] = 0xFE;
                      frame[1] = 0x01;
                      frame[2] = opusLen & 0xFF;
                      frame[3] = (opusLen >> 8) & 0xFF;
                      memcpy(frame + 4, p + off, opusLen);
                      sendBinaryNotification(frame, 4 + opusLen);
                  }
              }
          }
      }
      else if (current_mode == "TXT" && packet.type == "TXT") {
          if(packet.channel== channels[deviceSettings.channel_idx]) {
              // Store in inbox and display latest
              extern const char* bleGetDeviceIdShort();
              inboxStore(bleGetDeviceIdShort(), 8, (const uint8_t*)packet.content.c_str(), packet.content.length());
              
              updDisp(4, packet.content.c_str(), true);
              // Display GPS coordinates and timestamp below the message text
              if (packet.gpsData.length() > 0) {
                  char gps_display[32];
                  snprintf(gps_display, sizeof(gps_display), "GPS: %s", packet.gpsData.c_str());
                  updDisp(5, gps_display, false);
              } else {
                  updDisp(5, "No GPS data", false);
              }
              if (packet.sendDateTime.length() > 0) {
                  char dt_display[32];
                  snprintf(dt_display, sizeof(dt_display), "%s-%s-%s %s:%s:%s",
                           packet.sendDateTime.substring(0,4).c_str(),
                           packet.sendDateTime.substring(4,6).c_str(),
                           packet.sendDateTime.substring(6,8).c_str(),
                           packet.sendDateTime.substring(8,10).c_str(),
                           packet.sendDateTime.substring(10,12).c_str(),
                           packet.sendDateTime.substring(12,14).c_str());
                  updDisp(6, dt_display, false);
              }
          }
          updModeAndChannelDisplay();
      }
      else if (current_mode == "TXT" && packet.type == "TXT_MULTI") {
          if(packet.channel == channels[deviceSettings.channel_idx]) {
              // Extract seq/total from content: "1/N~{text}" or similar
              const char* content = packet.content.c_str();
              const char* slashPos = strchr(content, '/');
              const char* tildePos = strchr(slashPos ? (slashPos + 1) : content, '~');

              if (slashPos && tildePos && tildePos > slashPos) {
                  uint8_t seq = (uint8_t)atoi(content);
                  uint8_t total = (uint8_t)atoi(slashPos + 1);
                  const char* chunkData = tildePos + 1;
                  int chunkLen = strlen(chunkData);

                  // Check if this is a fresh batch or still within timeout
                  if (seq == 1 || (millis() - txt_reassemble_timer > TXT_MULTI_TIMEOUT_MS)) {
                      // Start new reassembly buffer
                      txt_reassemble_expected = total;
                      txt_reassemble_recv_count = 0;
                      txt_reassemble_buf[0] = '\0';
                      txt_reassemble_timer = millis();
                      updDisp(2, "Multi:", true);
                      updDisp(3, "Receiving", true);
                  }

                  // Append chunk data to buffer (memcpy for safety with null bytes)
                  int currentLen = strlen(txt_reassemble_buf);
                  if (currentLen + chunkLen < TXT_MULTI_MAX_MSG_LEN) {
                      memcpy(txt_reassemble_buf + currentLen, chunkData, chunkLen + 1);
                      txt_reassemble_recv_count++;

                      char status[32];
                      snprintf(status, sizeof(status), "%d/%d", txt_reassemble_recv_count, (int)txt_reassemble_expected);
                      updDisp(4, status, true);

                      if (txt_reassemble_recv_count == txt_reassemble_expected) {
                          // All chunks received — store full message in inbox and display
                          extern const char* bleGetDeviceIdShort();
                          inboxStore(bleGetDeviceIdShort(), 8, (const uint8_t*)txt_reassemble_buf, strlen(txt_reassemble_buf));
                          
                          updDisp(5, txt_reassemble_buf, true);
                          updDisp(6, "Full", true);
                          txtShowInbox = false;
                          txtInboxScrollPage = 0;
                      }
                  } else {
                      updDisp(3, "Overflow", true);
                  }
              }
          }
          updModeAndChannelDisplay();
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
            //Don't flood, just wait 3 seconds
            updDisp(5, "Ping!  3",true);
            delay(1000);
            updDisp(5, "Ping!  2",true);
            delay(1000);
            updDisp(5, "Ping!  1",true);
            delay(1000);
            updDisp(5, "Ping!",true);
            sendPacket("Ping!");
            updModeAndChannelDisplay();

      } 
      else if (current_mode == "RANGE" && packet.type == "RANGE") {
          if(packet.channel== channels[deviceSettings.channel_idx]) {
              //Let's work on the range
              if (packet.isRangeMessage()){
                  updDisp(3, packet.content.c_str(),true); //test message and counter

                  char display_msg[30];

                  snprintf(display_msg, sizeof(display_msg), "SNR: %.1f dB", radio->getSNR() );
                  updDisp(8, display_msg,false);
                  snprintf(display_msg, sizeof(display_msg), "RSSI: %.1f dBm", radio->getRSSI() );
                  updDisp(9, display_msg,true);          


                  if(range_last_count==0) {
                      //We just initialized, so reset the counter to what it now is
                      range_last_count=packet.packetCounter-1;
                  }

                  if(packet.packetCounter==range_last_count+1) {
                      //No Packet missed!
                      range_last_count++;
                      range_consecutive_ok++;
                  }
                  else {
                      //Packet loss!
                      int pckt_missed=packet.packetCounter-range_last_count+1;

                      if(pckt_missed<0) {
                          //Sender got reset so no miss
                          updDisp(6, "Sender was reset",false);
                      }
                      else {
                          range_total_pckt_loss+=pckt_missed;
                          range_consecutive_ok=0;
                      }

                      range_last_count=packet.packetCounter;
                  }

                  if(gps_status!=GPS_LOC) {
                      snprintf(display_msg, sizeof(display_msg), "No GPS Fx(%.0f)", gps_hdop );
                      updDisp(4, display_msg,false);
                      //Remove Wait for pckg
                      updDisp(6, "",false);
                  }
                  else {
                      if(range_home_lat==0 && range_home_long==0) {
                        //Let's set our homebase
                        range_home_lat=gps_latitude;
                        range_home_long=gps_longitude;
                        updDisp(4, "Home Location OK",true);
                      }
                      else {
                        snprintf(display_msg, sizeof(display_msg), "%.6f, %.6f", gps_latitude,gps_longitude );
                        updDisp(4, display_msg,false);
                      }

                      //Let's calculate the range
                      double distance = TinyGPSPlus::distanceBetween(range_home_lat, range_home_long, gps_latitude, gps_longitude);

                      Serial.print(F("Checking distance: "));
                      Serial.print(distance,6);
                      Serial.print(F(" - "));
                      Serial.print(range_home_lat,7);
                      Serial.print(F(" "));
                      Serial.print(range_home_long,7);
                      Serial.print(F(" / "));
                      Serial.print(gps_latitude,7);
                      Serial.print(F(" "));
                      Serial.print(gps_longitude,7);
                      Serial.println(F(" #"));

                      if(distance>0) {
                          if(range_max_dist < distance) {
                              range_max_dist = distance;
                          }
                          if(range_consecutive_ok>2) {
                              //We had no packet loss, so let's make it the stable distance
                              range_stable_dist=distance;
                          }
                      }

                      snprintf(display_msg, sizeof(display_msg), "Rng:%.1f/%.1fm", distance,range_max_dist);
                      updDisp(5, display_msg,false); 

                  }


                  //Let's print the info
                  snprintf(display_msg, sizeof(display_msg), "Stable: %.1fm", range_stable_dist);
                  updDisp(6, display_msg,false); //test message and counter

                  snprintf(display_msg, sizeof(display_msg), "PLoss: %d/%d ok", range_total_pckt_loss,range_consecutive_ok);
                  updDisp(7, display_msg,true); //test message and counter
                  updModeAndChannelDisplay();

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
            updDisp(4, " #Wait on GPS Fix",true);
        }
        updDisp(6, "Wait for pckg",true);
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
    ButtonConfig* config = ButtonConfig::getSystemButtonConfig();
    config->setEventHandler(handleEvent);
    config->setFeature(ButtonConfig::kFeatureLongPress);
    config->setFeature(ButtonConfig::kFeatureDoubleClick);
    config->setFeature(ButtonConfig::kFeatureSuppressAfterClick);
    config->setFeature(ButtonConfig::kFeatureSuppressAfterDoubleClick);

    config->setClickDelay(125);  

    modeButton.init(MODE_PIN);
    touchButton.init(TOUCH_PIN);
    
    while (Serial.available()) Serial.read();

    actionButtonTimer=millis();
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
            } else if (!strcmp(current_mode, "TXT") && txtInboxMsgCount > 0) {
                // In TXT mode with messages: toggle between inbox view and single-message view
                txtModeToggleInboxView();
            } else {
                // Single click cycles through modes (default behavior)
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

      if(gps_status!=GPS_LOC) {
          snprintf(display_msg, sizeof(display_msg), "No GPS Fx(%.0f)", gps_hdop );
          updDisp(3, display_msg,false);
      }
      else {
          snprintf(display_msg, sizeof(display_msg), "%.6f, %.6f", gps_latitude,gps_longitude );
          updDisp(3, display_msg,false);
      }

      snprintf(display_msg, sizeof(display_msg), "TmOnAr: %d", timeOnAir);
      updDisp(5, display_msg,true);

      updModeAndChannelDisplay();


      //Cool of period to allow receiving of messages because of switching from sent to receive takes time
      sendTestMessageTimer = millis();
    }

}


void sendTxtMessage(const char* message) {
    // Max payload per chunk after all overhead: header(~30) + TXM type(4) + channel(1) + seq/total(8) = ~43, leaving ~85 bytes for text
    #define TXT_CHUNK_SIZE 80
    int msgLen = strlen(message);
    char display_msg[64];

    // Auto-attach GPS coordinates and timestamp to TXT messages
    bool hasGPS = (gps_status == GPS_LOC);

    if (msgLen <= TXT_CHUNK_SIZE) {
        // Short message — send as a single packet (no chunking needed)
        // Format: TX{channel}~GP{lat},{lon}~ST{datetime}message or TX{channel}message
        char send_pkt_buf[TXT_CHUNK_SIZE + 16 + 80];
        if (hasGPS) {
            snprintf(send_pkt_buf, sizeof(send_pkt_buf), "TX%c~GP%.6f,%.6f~ST%s%s",
                     channels[deviceSettings.channel_idx],
                     gps_latitude, gps_longitude,
                     getFormattedDateTime().c_str(), message);
        } else {
            snprintf(send_pkt_buf, sizeof(send_pkt_buf), "TX%c%s", channels[deviceSettings.channel_idx], message);
        }

        snprintf(display_msg, sizeof(display_msg), "Sent: %s", message);
        updDisp(2, display_msg, true);

        extern void sendNotificationToApp(const char* msg);
        static char ackBuf[160];
        snprintf(ackBuf, sizeof(ackBuf), "LINE:TX|TEXT:Sent: %s", message);
        sendNotificationToApp(ackBuf);

        sendPacket(send_pkt_buf);
        delay(100);
        snprintf(display_msg, sizeof(display_msg), "TmOnAr: %d", timeOnAir);
        updDisp(3, display_msg, true);
    } else {
        // Long message — split into chunks with TXM multi-packet header
        int numChunks = (msgLen + TXT_CHUNK_SIZE - 1) / TXT_CHUNK_SIZE;

        // Notify phone of the full message upfront
        extern void sendNotificationToApp(const char* msg);
        static char ackBuf[256];
        snprintf(ackBuf, sizeof(ackBuf), "LINE:TX|MULTI:%d|%s", numChunks, message);
        sendNotificationToApp(ackBuf);

        updDisp(2, "Sent:", true);
        updDisp(3, "Chunks:", true);

        for (int i = 0; i < numChunks; i++) {
            int offset = i * TXT_CHUNK_SIZE;
            int remaining = msgLen - offset;
            int chunkLen = (remaining < TXT_CHUNK_SIZE) ? remaining : TXT_CHUNK_SIZE;

            // Format: TXM{channel}{seq}/{total}~GP{lat},{lon}~ST{datetime}~{content}
            char send_pkt_buf[MAX_PACKET_SIZE];
            if (hasGPS) {
                snprintf(send_pkt_buf, sizeof(send_pkt_buf), "TXM%c%d/%d~GP%.6f,%.6f~ST%s~",
                         channels[deviceSettings.channel_idx], i + 1, numChunks,
                         gps_latitude, gps_longitude,
                         getFormattedDateTime().c_str());
            } else {
                snprintf(send_pkt_buf, sizeof(send_pkt_buf), "TXM%c%d/%d~",
                         channels[deviceSettings.channel_idx], i + 1, numChunks);
            }

            // Append chunk content after the header
            memcpy(send_pkt_buf + strlen(send_pkt_buf), message + offset, chunkLen);
            send_pkt_buf[strlen(send_pkt_buf) + chunkLen] = '\0';

            sendPacket(send_pkt_buf);

            if (i < 5 || i == numChunks - 1) {
                char chunkDisplay[32];
                snprintf(chunkDisplay, sizeof(chunkDisplay), "%d/%d", i + 1, numChunks);
                updDisp(4, chunkDisplay, true);
            }

            // Brief gap between chunks to avoid radio contention
            if (i < numChunks - 1) {
                delay(200);
            }
        }

        snprintf(display_msg, sizeof(display_msg), "TmOnAr: %d", timeOnAir);
        updDisp(3, display_msg, true);
    }
    updModeAndChannelDisplay();
}

void sendTestMessage(bool now) {
    //Only do this every 5 seconds or when now=true
    if (millis() - sendTestMessageTimer > 5000 || now) {
        sendTestMessageTimer = millis();

        test_message_counter++;
        char test_msg[50];
        snprintf(test_msg, sizeof(test_msg), "test%d", test_message_counter);

        sendTxtMessage(test_msg);

        // Update mode and channel display after sending
        sendTestMessageTimer = millis();
    }
}


void updChannel() {
    updModeAndChannelDisplay();
}




void turnoffLed(){
    digitalWrite(GreenLed_Pin, HIGH);
    digitalWrite(RedLed_Pin, HIGH);
    digitalWrite(BlueLed_Pin, HIGH);
}

// ============================================================
// Text Inbox — TXT mode display and navigation
// ============================================================

void txtModeInboxDisplay() {
    uint8_t msg_count = inboxCount();
    if (msg_count == 0) {
        // No messages in inbox
        updDisp(2, "No messages yet", true);
        updDisp(3, "Wait for TXT mode", false);
        updDisp(4, "messages on this", false);
        updDisp(5, "channel.", false);
        return;
    }

    txtInboxMsgCount = msg_count;

    if (txtShowInbox) {
        // Show inbox page with scrollable list
        int lines_per_page = 16;  // rows 2-17
        int total_pages = (msg_count + lines_per_page - 1) / lines_per_page;
        
        if (txtInboxScrollPage >= total_pages) {
            txtInboxScrollPage = total_pages - 1;
        }

        updDisp(1, "Inbox:", true);
        
        // Show page header on row 2
        char page_header[32];
        snprintf(page_header, sizeof(page_header), "P %d/%d (%d msgs)", 
                 txtInboxScrollPage + 1, total_pages, msg_count);
        updDisp(2, page_header, true);

        // Clear the inbox display area
        for (int i = 3; i < 19; i++) {
            updDisp(i, "", false);
        }

        bool refresh = true;
        inboxDisplayPage(txtInboxScrollPage * lines_per_page, &txtInboxScrollPage, refresh);
    } else {
        // Show latest message view with count indicator
        char count_line[32];
        snprintf(count_line, sizeof(count_line), "%d msg(s)", msg_count);
        
        // Row 1: "TXT" mode label + "Inbox" toggle hint
        updDisp(1, "TXT Inbox", true);
        
        // Row 2: message count
        updDisp(2, count_line, false);
        
        // Clear remaining lines
        for (int i = 3; i < 19; i++) {
            updDisp(i, "", false);
        }

        // Show latest message preview at row 3
        inboxShowLatest(3);
    }
}

void txtModeToggleInboxView() {
    txtShowInbox = !txtShowInbox;
    
    // If scrolling in inbox mode, adjust page on toggle
    if (txtShowInbox) {
        int lines_per_page = 16;
        if (txtInboxScrollPage == 0 && txtInboxMsgCount > 1) {
            // Jump to last page when switching from single view
            txtInboxScrollPage = (txtInboxMsgCount + lines_per_page - 1) / lines_per_page - 1;
        } else if (!txtShowInbox) {
            txtInboxScrollPage = 0;
        }
    }
    
    // Redisplay after toggle
    txtModeInboxDisplay();
}

void txtModeClearInbox() {
    inboxClear();
    txtInboxScrollPage = 0;
    txtInboxMsgCount = 0;
    txtShowInbox = false;
    updDisp(2, "Inbox cleared", true);
    delay(1000);
    // Refresh to show empty state
    txtModeInboxDisplay();
}
