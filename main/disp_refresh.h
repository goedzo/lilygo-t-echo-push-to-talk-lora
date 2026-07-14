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

// Set partial window coordinates to be written to GRAM registers before waveform trigger.
// Call this with the same params as setPartialWindow() when doing partial updates.
void epdSetGRAMWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h);

// Call from main loop. Advances by ~0.5ms per call, checks BUSY pin every 10ms.
// Returns true on the first call after DONE (waveform just completed).
bool stepEpdRefresh();

// Manually power off (e.g. during sleep or when done with full refresh)
void epdPowerOffNow();

// Write a solid-color rectangle directly to SSD1681 GRAM and optionally trigger partial refresh.
// This writes pixel data directly to the e-ink controller memory without using GxEPD2's buffer model.
// color_byte: 0xFF for white, 0x00 for black (byte-per-bitmap format)
// Returns true when the refresh is complete (BUSY pin released).
bool epdWriteAndRefreshRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t color_byte);

#endif
