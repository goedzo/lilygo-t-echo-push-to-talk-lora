#ifndef SCAN_H
#define SCAN_H

#include <stdint.h>

// Define the structure to store frequency scan results
struct ChannelResult {
    float frequency;
    float rssi;
    float snr;
    int quality;  // Quality rating from 1-10
};

// Function declarations for frequency scan operations
void startScanFrequencies();
void stopScanFrequencies();
void handleFrequencyScan();
void printTopChannels();

#endif // SCAN_H
