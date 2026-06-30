# S9 — WP Mode Integration (Missing from modes array)

## Context

WP (Waypoint) is documented in `AGENTS.md` as one of the 9 modes but is missing from the `modes[]` array in `app_modes.cpp`. This story adds it, wires up display rendering, and ensures full mode cycling works end-to-end.

This is the smallest story — all layout logic already exists in S8 (8d). The work here is integration: adding WP to the mode list, initializing its LoRa config, and connecting the existing waypoint broadcast flow to the new display layout.

## Changes required

### 1. Add "WP" to modes array (`app_modes.cpp`)

```cpp
// Before:
const char* modes[] = { "BEACON","RAW","TXT", "RANGE", "TST","PONG","SCAN","PTT"};
const int numModes = sizeof(modes) / sizeof(modes[0]);  // = 8

// After:
const char* modes[] = { "BEACON","RAW","TXT", "RANGE", "TST","PONG","SCAN","PTT", "WP" };
const int numModes = sizeof(modes) / sizeof(modes[0]);  // = 9
```

### 2. Wire WP into `updMode()` init flow (`app_modes.cpp`)

In the existing mode-switch logic, add WP initialization:
```cpp
if(current_mode == "WP") {
    // Ensure LoRa is set for WP packet type
    setupLoRa();
}
```

### 3. Wire WP into `handleAppModes()` main loop (`app_modes.cpp`)

Add WP mode handling in the existing non-settings-mode block:
```cpp
else if (current_mode == "WP") {
    // Show current waypoint data
    drawWpLayout();  // from display_layout.cpp
    
    // Handle waypoint broadcast countdown
    if (wpBroadcastActive) {
        uint32_t elapsed = (millis() - wpBroadcastStartMs) / 1000;
        uint32_t remaining = 60 - elapsed;  // 60s broadcast window
        layout_state.wp_bcast_remaining_s = remaining;
        
        if (remaining == 0) {
            wpBroadcastActive = false;
        }
    }
}
```

### 4. Wire WP into `handlePacket()` for received waypoint packets (`app_modes.cpp`)

Add a case in the existing `handlePacket()` switch:
```cpp
else if (current_mode == "WP" && packet.type == "WAYPOINT") {
    // Parse and display received waypoint data
    // Update layout_state with received lat/lon/alt/label
}
```

### 5. Add WP icon (`display.cpp` or `display_layout.cpp`)

If no existing icon exists, add a simple crosshair/flag icon:
```cpp
const uint16_t wp_icon[16] = {
    // Simple flag/crosshair — white on black filled rect
    0b0000111100000000,
    0b0001000010000000,
    0b0001000010000000,
    0b0001111110000000,
    0b0001000010000000,
    0b0001000010000000,
    0b0001111110000000,
    0b0001000010000000,
    0b0001000010000000,
    0b0001000010000000,
    0b0001000010000000,
    0b0001111110000000,
    0b0001000010000000,
    0b0001000010000000,
    0b0000111100000000,
    0b0000000000000000,
};
```

And add to `drawModeIcon()` switch in `display.cpp`:
```cpp
else if (mode == "WP") {
    drawIcon(wp_icon, 0, disp_top_margin, disp_icon_height, disp_icon_width, GxEPD_WHITE, GxEPD_BLACK);
    icon_drawn = true;
}
```

### 6. Add WP mode to `updMode()` LoRa reinit check (`app_modes.cpp`)

In the existing conditional for LoRa reinit:
```cpp
if(current_mode=="TST" || current_mode=="RAW" || current_mode=="TXT" || 
   current_mode=="PONG" || current_mode=="RANGE" || current_mode=="SCAN" || 
   current_mode=="BEACON" || current_mode=="WP") {  // add WP here
    setupLoRa();
}
```

## What stays unchanged

- Waypoint data structure (`WaypointEntry` in `app_modes.h`) — already exists
- Waypoint storage in RTC RAM — already exists  
- Waypoint packet format: `WP~LA{lat},{lon}~AL{alt}~LB{label}~DI{id}` — already documented
- Broadcast timing (every 3s for 60s window) — already implemented in `startWaypointBroadcast()`
- `wpStoredWaypoints[]` and `wpStoredCount` — existing storage, no changes

## Testing & Verification

- [ ] `build_scripts\01_build_firmware.bat` compiles with zero errors
- [ ] MODE button cycles through all 9 modes including WP (previously 8)
- [ ] No duplicate or missing modes in the cycle
- [ ] WP mode: waypoint label shown big and centered on screen
- [ ] WP mode: GPS coords and altitude displayed correctly below label
- [ ] WP mode: broadcast countdown tag visible when broadcasting is active
- [ ] Flash usage in build output: ~30-32% (verify no regression from other stories)

## Risk

- **Mode cycle index mismatch:** If any code hardcodes `numModes` or assumes 8 modes (e.g., array bounds), adding WP may cause issues. Mitigation: search for all `numModes` and mode array references in the codebase before this story, update any hardcoded assumptions.

## Story Index Dependencies

- **Required before:** S1-S8 (all other stories)
- **Blocks:** nothing — this is the final integration story
