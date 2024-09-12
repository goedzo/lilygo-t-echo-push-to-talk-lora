#include "scan.h"
#include "lora.h"
#include "display.h"  // To use updDisp for e-ink display
#include <Arduino.h>

#define MAX_TOP_CHANNELS 10

// Variables to control the scanning process
float startFreq = 863.0; //869.46;
float endFreq = 869.65; //869.48;
float stepSize = 0.05;//0.01;
int numSamples = 10;
float currentFreq = startFreq;
int sampleCount = 0;
float rssiTotal = 0;
float snrTotal = 0;
bool scanning = false;
unsigned long lastScanTime = 0;
unsigned long scanInterval = 150;  // Time between samples in milliseconds
int displayLine = 2;  // Start at line 2 for displaying messages

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

// Function to calculate quality rating with 60-40 rule for SNR and RSSI
int calculateQuality(float rssi, float snr, bool ignoreSNR) {
    int rssiScore;
    int snrScore = 0;

    if (ignoreSNR) {
        // If SNR is unavailable, base the quality entirely on RSSI (scaled 0-100)
         rssiScore = map(rssi, -130, -20, 0, 100);  // RSSI scaled from 0 (worst) to 100 (best)
    } else {
        // When SNR is available, use 60-40 rule: SNR contributes 60%, RSSI 40%
        rssiScore = map(rssi, -130, -20, 0, 40);  // RSSI contributes up to 40 points
        snrScore = map(snr, -20, 10, 0, 60);      // SNR contributes up to 60 points
    }

    return constrain(rssiScore + snrScore, 1, 100);  // Total score out of 100
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
    currentFreq = startFreq;
    sampleCount = 0;
    rssiTotal = 0;
    snrTotal = 0;
    scanning = true;
    initTopChannels();
    Serial.println(F("Frequency scan started."));
}

// Stop the frequency scan
void stopScanFrequencies() {
    scanning = false;
    Serial.println(F("Frequency scan stopped."));
    //printTopChannels();  // Print the final top 10 channels to the display
}

// Handle the scanning process in the loop
void handleFrequencyScan() {
    if (scanning) {
        if (millis() - lastScanTime >= scanInterval) {
            lastScanTime = millis();

            if (sampleCount == 0) {
                // Set the current frequency only once, when sampleCount is 0 (i.e., for a new frequency)
                int state = radio->setFrequency(currentFreq);
                if (state != RADIOLIB_ERR_NONE) {
                    Serial.print(F("Failed to set frequency "));
                    Serial.print(currentFreq);
                    Serial.print(F(" MHz, code "));
                    Serial.println(state);
                    return;
                }
                // Start receiving
                radio->startReceive();
                char displayString[30];
                snprintf(displayString, sizeof(displayString), ">Test %.2f MHz", currentFreq);
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
                addResultToTopChannels(currentFreq, avgRSSI, avgSNR);

                printTopChannels();

                // Display results for current frequency
                char displayString[30];
                updDisp(2, "", false);  // Clear the scanning line

                snprintf(displayString, sizeof(displayString), "%.2f Q%d R%.1f", currentFreq, calculateQuality(avgRSSI, avgSNR,true), avgRSSI);
                updDisp(1, displayString, true);  // Print all info on one line

                // Move to the next frequency
                currentFreq += stepSize;
                sampleCount = 0;
                rssiTotal = 0;
                snrTotal = 0;

                // Stop scanning when done or out of space
                if (currentFreq <= endFreq) {  
                    displayLine++;  // Move to the next line for the next result
                } else {
                    //stopScanFrequencies();
                    //Wrap around
                    currentFreq=startFreq;
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
