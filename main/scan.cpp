#include "scan.h"
#include "lora.h"
#include "display.h"  // To use updDisp for e-ink display
#include <Arduino.h>

#define MAX_TOP_CHANNELS 10

// Variables to control the scanning process
int numSamples = 10;
int sampleCount = 0;
float rssiTotal = 0;
float snrTotal = 0;
bool scanning = false;
unsigned long lastScanTime = 0;
unsigned long scanInterval = 150;  // Time between samples in milliseconds
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
    //clearScreen();  // Clear the display before starting scan
    updDisp(1, "Scanning...", true);  // Display initial message
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
    Serial.println(F("Frequency scan started."));

    int state = setFrequency(currentFrequency);
    if (state != RADIOLIB_ERR_NONE) {
        Serial.print(F("Failed to set frequency "));
        Serial.print(currentFrequency);
        Serial.print(F(" MHz, code "));
        Serial.println(state);
    }


}

// Stop the frequency scan
void stopScanFrequencies() {
    if(scanning) {
        //We stop and revert back to the original frequency
        setFrequency(enterFrequency);
        scanning = false;
        Serial.println(F("Frequency scan stopped."));
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
                    Serial.print(F("Failed to set frequency "));
                    Serial.print(currentFrequency);
                    Serial.print(F(" MHz, code "));
                    Serial.println(state);
                    return;
                }
                // Start receiving
                radio->startReceive();
                char displayString[30];
                snprintf(displayString, sizeof(displayString), ">Test %.2f MHz", currentFrequency);
                updDisp(2, displayString, true);  // Print all info on one line

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

                // Display results for current frequency
                char displayString[30];
                updDisp(2, "", false);  // Clear the scanning line

                snprintf(displayString, sizeof(displayString), "%.2f Q%d R%.1f", currentFrequency, calculateQuality(avgRSSI, avgSNR,true), avgRSSI);
                updDisp(1, displayString, true);  // Print all info on one line

                // Move to the next frequency
                currentFrequency += stepSize;
                sampleCount = 0;
                rssiTotal = 0;
                snrTotal = 0;

                // Stop scanning when done or out of space
                if (currentFrequency <= endFreq) {  
                    displayLine++;  // Move to the next line for the next result
                } else {
                    //stopScanFrequencies();
                    //Wrap around
                    currentFrequency=startFreq;
                    displayLine++;
                }
            }
        }
    }
}

// Print the top 10 channels on the display
void printTopChannels() {
    for (int i = 0; i < MAX_TOP_CHANNELS && i < 7; i++) {  // Limit display to 7 entries
        char displayString[30];
        snprintf(displayString, sizeof(displayString), "%.2f Q%d R%.1f", topChannels[i].frequency, topChannels[i].quality, topChannels[i].rssi);
        if(i==6) {
            updDisp(3 + i, displayString, true);  // Print frequency, quality, and RSSI on one line
        }
        else {
            updDisp(3 + i, displayString, false);  // Print frequency, quality, and RSSI on one line
        }
    }
}
