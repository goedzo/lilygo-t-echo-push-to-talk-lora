// disp_refresh.cpp — Non-blocking e-paper waveform transfer
//
// Replaces blocking display->refresh() with a step function that advances the SSD1681
// waveform cycle in ~0.5ms increments per call, checked every 10ms via BUSY pin polling.
// The total wall-clock time is identical (~0.7s partial / ~3.8s full), but the CPU can
// do BLE/LoRa/button work between steps instead of being dead.
//
// This copies the exact waveform logic from GxEPD2_150_BN.cpp lines 390-409 and
// GxEPD2_EPD.cpp _PowerOn() so we don't need to modify vendored libraries.

#include "disp_refresh.h"
#include "utilities.h"
#include <Arduino.h>
#include <SPI.h>

// ── SPI settings (must match what setupDisplay() uses) ──
static SPIClass* s_spiclass = nullptr;
static SPISettings s_spi_settings;

// Partial window coordinates — set by renderPageLoop, written to GRAM registers before waveform
static uint16_t s_partial_x = 0;
static uint16_t s_partial_y = 0;
static uint16_t s_partial_w = 0;
static uint16_t s_partial_h = 0;
static bool   s_partial_set = false;

// Record partial window coordinates — call before triggerEpdRefresh() when doing partial updates.
// The coordinates are written to GRAM registers (0x44/0x45/0x4E/0x4F) during the waveform trigger,
// matching GxEPD2_150_BN::_setPartialRamArea() exactly so the SSD1681 refreshes the right region.
void epdSetGRAMWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    s_partial_x = x;
    s_partial_y = y;
    s_partial_w = w;
    s_partial_h = h;
    s_partial_set = true;
}

// ── Waveform LUT (copied from GxEPD2_150_BN.cpp:361-381) ──
static const uint8_t s_lut_partial[] = {
    0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x40, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x00, 0x00, 0x00,
};

// ── SPI helper functions (copied from GxEPD2_EPD.cpp) ──
static void _epd_write_command(uint8_t cmd) {
    s_spiclass->beginTransaction(s_spi_settings);
    digitalWrite(ePaper_Dc, LOW);
    digitalWrite(ePaper_Cs, LOW);
    s_spiclass->transfer(cmd);
    digitalWrite(ePaper_Cs, HIGH);
    digitalWrite(ePaper_Dc, HIGH);
    s_spiclass->endTransaction();
}

static void _epd_write_data(uint8_t data) {
    s_spiclass->beginTransaction(s_spi_settings);
    digitalWrite(ePaper_Cs, LOW);
    s_spiclass->transfer(data);
    digitalWrite(ePaper_Cs, HIGH);
    s_spiclass->endTransaction();
}

static void _epd_write_data(const uint8_t* data, uint16_t n) {
    s_spiclass->beginTransaction(s_spi_settings);
    digitalWrite(ePaper_Cs, LOW);
    for (uint16_t i = 0; i < n; i++) {
        s_spiclass->transfer(data[i]);
    }
    digitalWrite(ePaper_Cs, HIGH);
    s_spiclass->endTransaction();
}

// ── State machine enum ──
enum class EpdPhase {
    IDLE,            // Nothing to do
    POWER_RAMP,      // Waiting for power-on ramp (~100ms)
    TRIGGERED,       // Waveform triggered, polling BUSY pin
    DONE             // Waveform complete
};

static EpdPhase s_phase = EpdPhase::IDLE;
static uint32_t s_phase_start_ms = 0;
static bool s_use_full_refresh = false;
static bool s_partial_initialized = false;

// ── Public API ──

void initDispRefresh(SPIClass* spicls, SPISettings spi_set) {
    s_spiclass = spicls;
    s_spi_settings = spi_set;
}

bool isEpdRefreshing() {
    return s_phase != EpdPhase::IDLE && s_phase != EpdPhase::DONE;
}

void triggerEpdRefresh(bool full_refresh) {
    if (s_phase != EpdPhase::IDLE) return;  // Already refreshing — skip
    s_use_full_refresh = full_refresh;
    s_phase = EpdPhase::POWER_RAMP;
    s_phase_start_ms = millis();
}

bool stepEpdRefresh() {
    switch (s_phase) {
        case EpdPhase::IDLE:
            return false;  // Nothing happening

        case EpdPhase::DONE:
            s_phase = EpdPhase::IDLE;
            return true;   // Signal "just completed" on first call after done

        case EpdPhase::POWER_RAMP: {
            uint16_t ramp_time = s_use_full_refresh ? 100 : 100;  // power_on_time from GxEPD2_150_BN
            if (millis() - s_phase_start_ms >= ramp_time) {
                if (!s_use_full_refresh && s_partial_set) {
                    // For partial updates, write GRAM window + pointer registers so the SSD1681 knows
                    // which region to refresh. Mirrors GxEPD2_150_BN::_setPartialRamArea() exactly:
                    //   0x11 → 0x03 (RAM entry mode: x inc, y inc)
                    //   0x44 → x/8, (x+w-1)/8    (horizontal start/end in byte columns)
                    //   0x45 → y%256, y/256, (y+h-1)%256, (y+h-1)/256  (vertical start/end split 4 bytes)
                    //   0x4E → x/8    (RAM-X pointer)
                    //   0x4F → y%256, y/256     (RAM-Y pointer)
                    _epd_write_command(0x11);
                    _epd_write_data(0x03);

                    _epd_write_command(0x44);
                    _epd_write_data(uint8_t((s_partial_x / 8) & 0xFF));
                    _epd_write_data(((s_partial_x + s_partial_w - 1) / 8) & 0xFF);

                    _epd_write_command(0x45);
                    _epd_write_data(uint8_t(s_partial_y % 256));
                    _epd_write_data(uint8_t(s_partial_y / 256));
                    _epd_write_data(((s_partial_y + s_partial_h - 1) % 256));
                    _epd_write_data(((s_partial_y + s_partial_h - 1) / 256));

                    _epd_write_command(0x4e);
                    _epd_write_data(uint8_t((s_partial_x / 8) & 0xFF));

                    _epd_write_command(0x4f);
                    _epd_write_data(uint8_t(s_partial_y % 256));
                    _epd_write_data(uint8_t(s_partial_y / 256));

                    s_partial_set = false;
                }

                // Send display update command + data to SSD1681
                _epd_write_command(0x22);
                uint8_t cmd_byte = s_use_full_refresh ? 0xF7 : 0xFC;
                _epd_write_data(cmd_byte);

                // Trigger waveform (cmd 0x20)
                _epd_write_command(0x20);

                s_phase = EpdPhase::TRIGGERED;
                s_phase_start_ms = millis();
            }
            return false;
        }

        case EpdPhase::TRIGGERED: {
            // Poll BUSY pin — returns HIGH when waveform cycle is complete
            uint16_t busy_timeout = s_use_full_refresh ? 4000 : 800;  // ms from GxEPD2_150_BN
            if (digitalRead(ePaper_Busy) == HIGH) {
                // Waveform done — power off for partial refresh
                if (!s_use_full_refresh) {
                    _epd_write_command(0x22);
                    _epd_write_data(0x83);  // Power off command
                    _epd_write_command(0x20);

                    s_partial_x = 0;
                    s_partial_y = 0;
                    s_partial_w = 0;
                    s_partial_h = 0;
                }
                s_phase = EpdPhase::DONE;
                return false;
            }
            // Timeout guard
            if (millis() - s_phase_start_ms >= busy_timeout) {
                // Timed out — still mark done to avoid infinite hang
                s_phase = EpdPhase::DONE;
                return false;
            }
            // Check every 10ms for responsiveness
            static uint32_t last_check = 0;
            if (millis() - last_check >= 10) {
                last_check = millis();
            }
            return false;
        }
    }

    return false;
}

void epdPowerOffNow() {
    _epd_write_command(0x22);
    _epd_write_data(0x83);
    _epd_write_command(0x20);
    s_phase = EpdPhase::IDLE;
}
