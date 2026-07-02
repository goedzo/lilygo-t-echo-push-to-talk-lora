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
#include "screen_sync.h"
#include "display_layout.h"  // For per-mode drawXxxLayout() wiring

// Extern for partial/full refresh control
extern void forceFullRefresh();

using namespace ace_button;

// Define an array of mode names as strings
const char* modes[] = { "BEACON","RAW","TXT", "RANGE", "TST","PONG","SCAN","PTT"};
const int numModes = sizeof(modes) / sizeof(modes[0]);
int modeIndex = 2;
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

// Beacon mode distance tracking
double beacon_display_dist = -1;
String beacon_display_name;
unsigned long beacon_last_distance_update = 0;

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
#define MODE_PIN _PINNUM(1,10)  // Button 2 connected to P1.10
#define TOUCH_PIN _PINNUM(0,11)  // Button 2 connected to P0.11 (Touch-capable pin)
AceButton modeButton(MODE_PIN);
AceButton touchButton(TOUCH_PIN);

// Custom ButtonConfig that reads MODE_PIN via direct GPIO register to avoid
// softdevice GPIOTE interference on nRF52 P1 port pins (digitalRead returns stale HIGH)
class ButtonConfigModePin : public ace_button::ButtonConfig {
  public:
    int readButton(uint8_t pin) override {
      if (pin == MODE_PIN) {
        // Use nrf_gpio_pin_read — handles P0/P1 transparently and bypasses softdevice GPIOTE lock
        return nrf_gpio_pin_read(MODE_PIN) ? HIGH : LOW;
      }
      return ButtonConfig::readButton(pin);
    }
};

ButtonConfigModePin g_modeButtonConfig;  // Custom config with direct GPIO read for MODE_PIN

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


    // Force full refresh on mode switch — clears ghosting from previous layout
    forceFullRefresh();

    // Update the mode and channel display — drawDefaultLayout() handles full refresh on mode switch
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

    // Update GPS location
    loopGPS();

    // Send screen mirror to companion app (throttled to 1/sec)
    sendScreenSync();

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
            
            // Update TST layout state and render (every loop to keep display current)
            layout_state.tst_sent = test_message_counter;
            layout_state.tst_rcvd = pckt_count;
            
            // Only redraw every 2 seconds or when counters change — prevents e-paper flash damage
            static uint32_t last_tst_draw = 0;
            if (millis() - last_tst_draw > 2000) {
                drawTstLayout();
                last_tst_draw = millis();
            }
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
              // Start pingpong — update state and render
              layout_state.pong_state = 1;  // sending
              drawPongLayout();
              sendPacket("Ping!");
            }
            
            // Render current PONG state — only when state changed or every 3s
            static uint32_t last_pong_draw = 0;
            static int8_t last_pong_state = -1;
            if (millis() - last_pong_draw > 3000 || layout_state.pong_state != last_pong_state) {
                drawPongLayout();
                last_pong_draw = millis();
                last_pong_state = layout_state.pong_state;
            }
        }
        else if (current_mode == "RANGE") {
            if (debouncedTouchPress()) {
                range_role_sender=!range_role_sender;
                //Reset the counters
                range_last_count=0;
                range_total_pckt_loss=0;
                range_consecutive_ok=0;
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
            
            // Render current scan state after processing — only when progress changes
            static uint32_t last_scan_draw = 0;
            static int16_t last_scan_progress = -1;
            {
                float freq_range = endFreq - startFreq;
                int16_t cur_progress = (scanning) ? (int16_t)((currentFrequency - startFreq) / freq_range * 100) : 0;
                if (millis() - last_scan_draw > 1000 || cur_progress != last_scan_progress) {
                    drawScanLayout();
                    last_scan_draw = millis();
                    last_scan_progress = cur_progress;
                }
            }
        }
        
        else if (current_mode == "BEACON") {
            // Find the closest peer with GPS for distance readout
            double bestDist = -1;
            String bestName;
            unsigned long now = millis();
            
            for (int i = 0; i < peerRosterCount; i++) {
                if (peerRoster[i].distanceM > 0 && peerRoster[i].distanceM < 50000) {
                    if (bestDist < 0 || peerRoster[i].distanceM < bestDist) {
                        bestDist = peerRoster[i].distanceM;
                        const char* displayName = peerRoster[i].callSign[0] != '\0' ? peerRoster[i].callSign : peerRoster[i].deviceId.c_str();
                        bestName = displayName;
                    }
                }
            }
            
            // Update distance display if we have a valid peer
            if (bestDist >= 0) {
                beacon_display_dist = bestDist;
                beacon_display_name = bestName;
                beacon_last_distance_update = now;
            }
            
            // Render BEACON layout via frame engine — only when peer data changes or every 3s
            static uint32_t last_beacon_draw = 0;
            static double last_beacon_dist = -999;
            if (millis() - last_beacon_draw > 3000 || beacon_display_dist != last_beacon_dist) {
                drawBeaconLayout();
                last_beacon_draw = millis();
                last_beacon_dist = beacon_display_dist;
            }
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
      if (current_mode == "RAW") {
          strncpy(layout_state.raw_hex_line1, packet.content.c_str(), sizeof(layout_state.raw_hex_line1) - 1);
          layout_state.raw_hex_line1[sizeof(layout_state.raw_hex_line1) - 1] = '\0';
          drawRawLayout();
      }
    } 
    else {
      // Parse the packet using the current channel configuration
      if (current_mode == "BEACON" && packet.type == "BEACON") {
          // BEACON mode: add peer to roster and display it
          beaconAddOrUpdate(packet);
          
          // Re-scan for closest peer with distance after adding/updating roster
          double bestDist = -1;
          String bestName;
          for (int i = 0; i < peerRosterCount; i++) {
              if (peerRoster[i].distanceM > 0 && peerRoster[i].distanceM < 50000) {
                  if (bestDist < 0 || peerRoster[i].distanceM < bestDist) {
                      bestDist = peerRoster[i].distanceM;
                      const char* dn = peerRoster[i].callSign[0] != '\0' ? peerRoster[i].callSign : peerRoster[i].deviceId.c_str();
                      bestName = dn;
                  }
              }
          }
          
          // Update distance display immediately
          if (bestDist >= 0) {
              beacon_display_dist = bestDist;
              beacon_display_name = bestName;
          }
          
          // Render updated BEACON layout via frame engine
          drawBeaconLayout();
      }
      else if (current_mode == "RAW" || current_mode == "TST") {
          //Cool of period to allow receiving of messages because of switching from sent to receive takes time
          sendTestMessageTimer = millis();

          pckt_count++;
          
          if (current_mode == "RAW") {
              // Set raw layout state for drawRawLayout()
              strncpy(layout_state.raw_hex_line1, packet.content.c_str(), sizeof(layout_state.raw_hex_line1) - 1);
              layout_state.raw_hex_line1[sizeof(layout_state.raw_hex_line1) - 1] = '\0';
              drawRawLayout();
          } else {
              // TST mode — update test counters for drawTstLayout()
              layout_state.tst_sent = test_message_counter;
              layout_state.tst_rcvd = pckt_count;
              drawTstLayout();
          }
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
                      
                      // Mark PTT RX state for drawPttLayout()
                      setPttRxActive(true);
                      drawPttLayout();
                      
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
              
              // Render TXT single message view via frame engine
              drawTxtSingleLayout();
          }
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
                  }

                  // Append chunk data to buffer (memcpy for safety with null bytes)
                  int currentLen = strlen(txt_reassemble_buf);
                  if (currentLen + chunkLen < TXT_MULTI_MAX_MSG_LEN) {
                      memcpy(txt_reassemble_buf + currentLen, chunkData, chunkLen + 1);
                      txt_reassemble_recv_count++;

                      if (txt_reassemble_recv_count == txt_reassemble_expected) {
                          // All chunks received — store full message in inbox and display
                          extern const char* bleGetDeviceIdShort();
                          inboxStore(bleGetDeviceIdShort(), 8, (const uint8_t*)txt_reassemble_buf, strlen(txt_reassemble_buf));
                          
                          txtShowInbox = false;
                          txtInboxScrollPage = 0;
                      }
                  }
              }
          }
      }
      else if (current_mode == "PONG" && packet.type == "PING") {
          //We pong this message
          
          // Print RSSI (Received Signal Strength Indicator)
          Serial.print(F("[SX1262] RSSI:\t\t"));
          Serial.print(radio->getRSSI());
          Serial.println(F(" dBm"));

          // Print SNR (Signal-to-Noise Ratio)
          Serial.print(F("[SX1262] SNR:\t\t"));
          Serial.print(radio->getSNR());
          Serial.println(F(" dB"));

          layout_state.pong_state = 2;  // received
          layout_state.pong_rtt_ms = 0;  // Set to actual RTT when available
          
          drawPongLayout();

          // Send another packet
          Serial.print(F("[SX1262] Sending another packet ... "));
          //Don't flood, just wait 3 seconds
          delay(1000);
          layout_state.pong_state = 1;  // sending again
          drawPongLayout();
          sendPacket("Ping!");
      } 
      else if (current_mode == "RANGE" && packet.type == "RANGE") {
          if(packet.channel== channels[deviceSettings.channel_idx]) {
              //Let's work on the range
              if (packet.isRangeMessage()){
                  char display_msg[30];

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
                      }
                      else {
                          range_total_pckt_loss+=pckt_missed;
                          range_consecutive_ok=0;
                      }

                      range_last_count=packet.packetCounter;
                  }

                  if(gps_status!=GPS_LOC) {
                      snprintf(display_msg, sizeof(display_msg), "No GPS Fx(%.0f)", gps_hdop );
                  }
                  else {
                      if(range_home_lat==0 && range_home_long==0) {
                        //Let's set our homebase
                        range_home_lat=gps_latitude;
                        range_home_long=gps_longitude;
                      }
                      else {
                        snprintf(display_msg, sizeof(display_msg), "%.6f, %.6f", gps_latitude,gps_longitude );
                      }

                      //Let's calculate the range
                      double distance = TinyGPSPlus::distanceBetween(range_home_lat, range_home_long, gps_latitude, gps_longitude);

                      if(distance>0) {
                          if(range_max_dist < distance) {
                              range_max_dist = distance;
                          }
                          if(range_consecutive_ok>2) {
                              //We had no packet loss, so let's make it the stable distance
                              range_stable_dist=distance;
                          }
                      }
                  }

                  // Update range layout state and render
                  layout_state.range_distance_m = (int)range_stable_dist;
                  layout_state.range_sender = range_role_sender;
                  drawRangeLayout();

              }
              else {
                  showError("no range pckt");
              }
          }
      }
    }
}


void printRangeStatus() {
    // Update range layout state and render
    layout_state.range_sender = range_role_sender;
    drawRangeLayout();
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
    // Re-ensure MODE_PIN is INPUT_PULLUP — may have been changed by Bluefruit or LoRa init
    pinMode(MODE_PIN, INPUT_PULLUP);
    pinMode(TOUCH_PIN, INPUT_PULLUP);

    // Verify pull-up is working: should read HIGH (unpressed) at boot
    SerialMon.print("[BTN] Boot check MODE_PIN= ");
    SerialMon.println(digitalRead(MODE_PIN) ? "HIGH (OK)" : "LOW (button stuck?)");

    // Use standard ButtonConfig for touch button
    ButtonConfig* config = ButtonConfig::getSystemButtonConfig();
    g_modeButtonConfig.setEventHandler(handleEvent);
    g_modeButtonConfig.setFeature(ButtonConfig::kFeatureLongPress);
    g_modeButtonConfig.setFeature(ButtonConfig::kFeatureDoubleClick);
    g_modeButtonConfig.setFeature(ButtonConfig::kFeatureSuppressAfterClick);
    g_modeButtonConfig.setFeature(ButtonConfig::kFeatureSuppressAfterDoubleClick);

    g_modeButtonConfig.setClickDelay(125);  

    modeButton.setButtonConfig(&g_modeButtonConfig);
    touchButton.init(TOUCH_PIN);
    
    while (Serial.available()) Serial.read();

    actionButtonTimer=millis();
}

void handleEvent(ace_button::AceButton* button, uint8_t eventType, uint8_t buttonState) {
    if (button->getPin() == MODE_PIN) {
        if (eventType == AceButton::kEventPressed) {
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

      sendPacket(send_pkt_buf);
      //Give the device some small time to process sending
      delay(100);

      if(gps_status!=GPS_LOC) {
          snprintf(display_msg, sizeof(display_msg), "No GPS Fx(%.0f)", gps_hdop );
      }
      else {
          snprintf(display_msg, sizeof(display_msg), "%.6f, %.6f", gps_latitude,gps_longitude );
      }

      // Render current mode layout after sending — use per-mode layout instead of generic default
      if (current_mode == "RANGE") {
          layout_state.range_sender = range_role_sender;
          drawRangeLayout();
      } else {
          drawDefaultLayout();
      }


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

        extern void sendNotificationToApp(const char* msg);
        static char ackBuf[160];
        snprintf(ackBuf, sizeof(ackBuf), "LINE:TX|TEXT:Sent: %s", message);
        sendNotificationToApp(ackBuf);

        sendPacket(send_pkt_buf);
        delay(100);
        
        // Render TXT single message view via frame engine
        drawTxtSingleLayout();
    } else {
        // Long message — split into chunks with TXM multi-packet header
        int numChunks = (msgLen + TXT_CHUNK_SIZE - 1) / TXT_CHUNK_SIZE;

        // Notify phone of the full message upfront
        extern void sendNotificationToApp(const char* msg);
        static char ackBuf[256];
        snprintf(ackBuf, sizeof(ackBuf), "LINE:TX|MULTI:%d|%s", numChunks, message);
        sendNotificationToApp(ackBuf);

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
                
                // Update TXT inbox count and render
                txtInboxMsgCount = inboxCount();
                drawTxtSingleLayout();
            }

            // Brief gap between chunks to avoid radio contention
            if (i < numChunks - 1) {
                delay(200);
            }
        }

        // Final TXT layout render after all chunks sent
        txtInboxMsgCount = inboxCount();
        drawTxtSingleLayout();
    }
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
        // No messages in inbox — render empty state
        layout_state.mode = current_mode;
        drawTxtSingleLayout();
        return;
    }

    txtInboxMsgCount = msg_count;

    if (txtShowInbox) {
        // Show inbox page with scrollable list — use drawTxtInboxLayout()
        int lines_per_page = 16;  // rows 2-17
        int total_pages = (msg_count + lines_per_page - 1) / lines_per_page;
        
        if (txtInboxScrollPage >= total_pages) {
            txtInboxScrollPage = total_pages - 1;
        }

        drawTxtInboxLayout();
    } else {
        // Show latest message view — use drawTxtSingleLayout()
        drawTxtSingleLayout();
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
    
    // Render TXT inbox view showing empty state via frame engine
    drawTxtSingleLayout();
    delay(1000);
    // Refresh to show empty state
    txtModeInboxDisplay();
}
