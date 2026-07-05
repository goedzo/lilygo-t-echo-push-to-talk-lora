// disp_refresh.h — Non-blocking e-paper waveform transfer API

#ifndef DISP_REFRESH_H
#define DISP_REFRESH_H

#include <Arduino.h>
#include <SPI.h>

// Call once from setupDisplay() to register SPI class and settings
void initDispRefresh(SPIClass* spicls, SPISettings spi_set);

// Returns true while a waveform cycle is in progress (after triggerEpdRefresh, before DONE)
bool isEpdRefreshing();

// Start the waveform cycle. total wall-clock time is identical to blocking refresh.
// Use after drawing is complete (buffer pushed to SSD1681 via firstPage/nextPage).
void triggerEpdRefresh(bool full_refresh);

// Call from main loop. Advances by ~0.5ms per call, checks BUSY pin every 10ms.
// Returns true on the first call after DONE (waveform just completed).
bool stepEpdRefresh();

// Manually power off (e.g. during sleep or when done with full refresh)
void epdPowerOffNow();

#endif
