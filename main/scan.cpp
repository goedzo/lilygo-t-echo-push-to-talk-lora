#include "scan.h"
#include "lora.h"
#include "display.h"  // To use updDisp for e-ink display
#include "display_layout.h"  // For layout_state access
#include <Arduino.h>

extern void sendSerialToApp(const String& msg);
extern void sendSerialToAppLn(const String& msg);
extern void sendScreenSyncIfDirty();

#define MAX_TOP_CHANNELS 10

// Variables to control the scanning process
int numSamples = 10;
int sampleCount = 0;
float rssiTotal = 0;
float snrTotal = 0;
bool scanning = false;
unsigned long lastScanTime = 0;
unsigned long scanInterval = 150;  // Time between samples in milliseconds
unsigned long scanLastNotifTime = 0;  // Rate-limit: BLE notifications for scan data
int displayLine = 2;  // Start at line 2 for displaying messages
float enterFrequency = defaultFrequency; //If we exit scan, we must go back to the enter freq


// Array to store the top 10 results
ChannelResult topChannels[MAX_TOP_CHANNELS];

// Initialize top channels
void initTopChannels() {
    for (int i = 0; i < MAX_TOP_CHANNELS; i++) {
        topChannels[i] = {0, -999, -999, 0};  // Invalid values to start
    }
    displayLine = 2;  // Reset the display line counter
    printTopChannels();
    syncTopChannelsToLayout();  // Clear layout state when reinitializing
}

// Add result to the top channels list
void addResultToTopChannels(float frequency, float rssi, float snr) {
    int quality = calculateQuality(rssi, snr,true);

    // Find the worst quality result
    int worstIndex = 0;
    for (int i = 1; i < MAX_TOP_CHANNELS; i++) {
        if (topChannels[i].quality < topChannels[worstIndex].quality) {
            worstIndex = i;
        }
    }

    // Replace the worst entry if the new quality is better
    if (quality > topChannels[worstIndex].quality) {
        topChannels[worstIndex] = {frequency, rssi, snr, quality};
    }

    // Sort the list by quality
    for (int i = 0; i < MAX_TOP_CHANNELS - 1; i++) {
        for (int j = i + 1; j < MAX_TOP_CHANNELS; j++) {
            if (topChannels[j].quality > topChannels[i].quality) {
                ChannelResult temp = topChannels[i];
                topChannels[i] = topChannels[j];
                topChannels[j] = temp;
            }
        }
    }
}

// Start the frequency scan
void startScanFrequencies() {
    //Save the entry frequency
    enterFrequency=currentFrequency;

    currentFrequency = startFreq;
    sampleCount = 0;
    rssiTotal = 0;
    snrTotal = 0;
    scanning = true;
    initTopChannels();
    syncTopChannelsToLayout();  // Clear layout state on scan start
    sendSerialToAppLn(F("Frequency scan started."));

    int state = setFrequency(currentFrequency);
    if (state != RADIOLIB_ERR_NONE) {
        sendSerialToApp(F("Failed to set frequency "));
        sendSerialToApp((String)currentFrequency);
        sendSerialToApp(F(" MHz, code "));
        sendSerialToAppLn((String)state);
    }


}

// Stop the frequency scan
void stopScanFrequencies() {
    if(scanning) {
        //We stop and revert back to the original frequency
        setFrequency(enterFrequency);
        scanning = false;
        sendSerialToAppLn(F("Frequency scan stopped."));
    }
    //printTopChannels();  // Print the final top 10 channels to the display
}

// Handle the scanning process in the loop
void handleFrequencyScan() {
    if (scanning) {
        if (millis() - lastScanTime >= scanInterval) {
            lastScanTime = millis();

            if (sampleCount == 0) {
                // Set the current frequency only once, when sampleCount is 0 (i.e., for a new frequency)
                int state = setFrequency(currentFrequency);
                if (state != RADIOLIB_ERR_NONE) {
                    sendSerialToApp(F("Failed to set frequency "));
                    sendSerialToApp((String)currentFrequency);
                    sendSerialToApp(F(" MHz, code "));
                    sendSerialToAppLn((String)state);
                    return;
                }
                // Start receiving
                radio->startReceive();

            }


            if (sampleCount < numSamples) {
                // Small delay to allow the receiver to adjust and capture RSSI/SNR data
                delay(20);  // Allow the radio some time to receive signals before measuring
                // Get RSSI and SNR values
                float rssi = radio->getRSSI(false);
                float snr = radio->getSNR();  // Capture SNR but don't display

                // Check if RSSI is valid
                if (rssi != 0) {
                    rssiTotal += rssi;
                    snrTotal += snr;
                    sampleCount++;
                }
            } else {
                // Calculate the average RSSI and SNR for this frequency
                float avgRSSI = rssiTotal / numSamples;
                float avgSNR = snrTotal / numSamples;

                // Add result to top channels
                addResultToTopChannels(currentFrequency, avgRSSI, avgSNR);

                printTopChannels();
                // Rate-limit BLE sync: only push scan data to companion app every 3s
                if (scanning && millis() - scanLastNotifTime >= SCAN_NOTIF_INTERVAL_MS) {
                    syncTopChannelsToLayout();
                    scanLastNotifTime = millis();
                    // Push screen sync to companion app so the app display updates during scanning
                    sendScreenSyncIfDirty();
                } else {
                    // Still update layout state for local rendering, but skip BLE sync
                    auto& S = layout_state;
                    uint8_t count = MAX_TOP_CHANNELS;
                    for (int i = 0; i < MAX_TOP_CHANNELS; i++) {
                        if (topChannels[i].rssi <= -998) { count = (uint8_t)i; break; }
                    }
                    S.scan_channel_count = count;
                    for (uint8_t i = 0; i < count; i++) {
                        S.scan_channels[i].frequency = topChannels[i].frequency;
                        S.scan_channels[i].quality   = topChannels[i].quality;
                        S.scan_channels[i].rssi      = topChannels[i].rssi;
                    }
                }

                sendSerialToAppLn(String("SCAN ") + String(currentFrequency, 2) + "MHz Q" + String(calculateQuality(avgRSSI, avgSNR,true)) + " R" + String(avgRSSI, 1) + " S" + String(avgSNR, 1));

                // Move to the next frequency
                currentFrequency += stepSize;
                sampleCount = 0;
                rssiTotal = 0;
                snrTotal = 0;

                // Stop scanning when done or out of space
                if (currentFrequency <= endFreq) {  
                    displayLine++;  // Move to the next line for the next result
                } else {
                    //Wrap around
                    currentFrequency=startFreq;
                    displayLine++;
                }
            }
        }
    }
}

// Sync top channels into layout_state for drawScanLayout()
void syncTopChannelsToLayout(void) {
    auto& S = layout_state;
    uint8_t count = MAX_TOP_CHANNELS;
    // Count how many entries have valid RSSI (better than initial -999)
    for (int i = 0; i < MAX_TOP_CHANNELS; i++) {
        if (topChannels[i].rssi <= -998) {
            count = (uint8_t)i;
            break;
        }
    }
    layout_state.scan_channel_count = count;
    for (uint8_t i = 0; i < count; i++) {
        layout_state.scan_channels[i].frequency = topChannels[i].frequency;
        layout_state.scan_channels[i].quality   = topChannels[i].quality;
        layout_state.scan_channels[i].rssi       = topChannels[i].rssi;
    }
}

// Print the top 10 channels on the display (legacy — kept for debug)
void printTopChannels() {
    syncTopChannelsToLayout();
}
