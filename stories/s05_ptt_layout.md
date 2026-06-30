# S5 — PTT Mode Layout (Big Card TX/RX State Block)

## Context

PTT mode relays Opus audio frames from BLE ↔ LoRa. Current display shows only minimal info ("Mode: PTT", channel bitrate). This story adds the Big Card layout: a bordered state block with inverted background for unmistakable visual signal indication — white-bg/black-text when receiving, black-bg/white-text when transmitting.

**No changes to audio frame handling in `ble.cpp` or LoRa packet forwarding.** Pure display change. The state detection comes from existing PTT receive/transmit flags.

## Layout spec (exact pixel positions)

```
y=12  [txrx_icon] ┌─AUDIO RELAY─┐  chn:A spf:7      ← header row (shared, uses txrx icon if needed or reuse ptt_icon)
y=40                                                 
y=48    ┌──────────────────┐                           ← black-bordered rect (outer border box)
y=52    │                  │
y=56    │  RECEIVING       │                           ← white bg / black text — normal state
y=60    │                  │
y=76    └──────────────────┘
y=104                                                    
y=120   chn: B spf: 7                                    ← channel/SF (small monospace)
y=136   SNR: -9.5dB RSSI: -67dBm                       ← signal metrics
```

### State block variations (background inversion = critical visual cue)

| State | Background | Text | Content |
|---|---|---|---|
| Idle (no audio flow) | WHITE | BLACK | "RECEIVING" (standby, awaiting incoming) |
| Receiving audio | WHITE | BLACK | "RECEIVING" + maybe volume bars or audio level indicator if feasible |
| Transmitting audio | BLACK | WHITE | "SENDING ●●●" (filled circle dots as visual activity indicator) |

### Rules

- The state block IS the primary element — its background inversion creates the unmistakable visual signal
- When transmitting: fill the block rect with BLACK first, then draw white text inside. This is safe for e-ink because it's a full-region draw within a previously-primed area.
- Channel/SF line on separate row below block (not part of state block)
- SNR/RSSI shown as secondary info

## State detection (from existing code paths)

The transmit/receive state is tracked by existing PTT code:
- **Transmitting:** when `receiveOpusFrame()` or equivalent forwards Opus bytes to LoRa via `sendPacket()` with PTT packet type
- **Receiving:** when a PTT packet arrives and Opus bytes are forwarded via BLE binary notification

New state variables (local to display_layout.cpp or added to LayoutState):
```cpp
bool ptt_tx_active;      // true during transmission
bool ptt_rx_active;      // true during reception  
unsigned long ptt_state_changed_ms;  // for debounce/throttle
```

These get set from existing PTT code in `app_modes.cpp` — the display only READS these values.

## Data sources (existing, unchanged)

- `deviceSettings.channel_idx`, `deviceSettings.bitrate_idx` — channel/SF or bitrate
- `channels[]` array for channel letter mapping
- `radio->getSNR()`, `radio->getRSSI()` for signal metrics
- Opus forward logic in `ble.cpp` / `app_modes.cpp` (PTT transmit/receive detection)

## Implementation

In `display_layout.cpp`:

```cpp
void drawPttLayout() {
    // Determine current state
    char state_text[20];
    uint16_t bg_color, text_color;
    
    if (ptt_tx_active) {
        strcpy(state_text, "SENDING ");
        // Add ●●● dots as visual activity
        bg_color = GxEPD_BLACK;
        text_color = GxEPD_WHITE;
    } else if (ptt_rx_active) {
        strcpy(state_text, "RECEIVING");
        bg_color = GxEPD_WHITE;
        text_color = GxEPD_BLACK;
    } else {
        strcpy(state_text, "STANDBY");
        bg_color = GxEPD_WHITE;
        text_color = GxEPD_BLACK;
    }

    // State block at y=48 — bordered rect
    int blk_x = 50, blk_y = 48, blk_w = 100, blk_h = 40;
    
    // Fill background first (critical for e-ink)
    display->fillRect(blk_x, blk_y, blk_w, blk_h, bg_color);
    
    // Draw border
    display->drawRect(blk_x, blk_y, blk_w, blk_h, GxEPD_BLACK);
    
    // Set cursor to center of block
    int cx = blk_x + (blk_w - 10) / 2;  // slight left offset for cursor alignment
    int cy = blk_y + (blk_h - 16) / 2 + 4;  // center vertically
    
    display->setCursor(cx, cy);
    display->fillRect(blk_x, blk_y, blk_w, blk_h, bg_color);  // ensure bg is set before text
    display->setTextColor(text_color);
    display->setFont(&FreeMonoBold9pt7b);
    
    // State text
    display->print(state_text);

    // Channel/SF below block at y=120
    char chan_buf[30];
    if (current_mode == "PTT") {
        snprintf(chan_buf, sizeof(chan_buf), "chn: %c %dbps", 
                 channels[deviceSettings.channel_idx], 
                 getBitrateFromIndex(deviceSettings.bitrate_idx));
    } else {
        snprintf(chan_buf, sizeof(chan_buf), "chn: %c spf: %d", 
                 channels[deviceSettings.channel_idx], deviceSettings.spreading_factor);
    }
    drawSecondaryRow(chan_buf, 120);

    // Signal metrics at y=136
    char sig_buf[50];
    snprintf(sig_buf, sizeof(sig_buf), "SNR: %.1fdB RSSI: %.1fdBm", 
             radio->getSNR(), radio->getRSSI());
    drawSecondaryRow(sig_buf, 136);

    // Clear remaining lines
}
```

## Testing & Verification

- [ ] `build_scripts\01_build_firmware.bat` compiles with zero errors
- [ ] On device: state block clearly visible in white-bg/black-text (receiving) or black-bg/white-text (sending)
- [ ] Background inversion is unmistakable when PTT switches between TX/RX
- [ ] Channel/SF and signal metrics update correctly below block
- [ ] Flicker-free state transitions via partial window of state block region only

## What stays unchanged

- Opus frame receiving in `ble.cpp` (BLE GATT notification)
- Opus frame forwarding to LoRa via packet type "PT"
- BLE binary notification format (`0xFE 0x01 + length + payload`)
- Audio pipeline end-to-end — only the display changes

## Story Index Dependencies

- **Required before:** S1 (primitives), S2 (frame engine)
- **Blocks:** none (independent of other mode stories)
