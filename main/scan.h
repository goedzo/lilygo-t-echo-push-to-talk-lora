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

// Rate limiting: only send scan data to central once per interval
extern unsigned long scanLastNotifTime;
#define SCAN_NOTIF_INTERVAL_MS 3000   // Only notify companion app every 3 seconds

#endif // SCAN_H
