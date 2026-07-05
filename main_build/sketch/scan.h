#line 1 "V:\\Bmad\\Project_ptt_lora\\main\\scan.h"
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
void initTopChannels();
void stopScanFrequencies();
void handleFrequencyScan();
void printTopChannels();

// Scan module state (used by display layout)
extern bool scanning;

// Sync top channels into layout_state for drawScanLayout()
void syncTopChannelsToLayout(void);

#endif // SCAN_H
