# S2 — Frame Engine + E-ink Safe Rendering

## Context

The current `updDisp()` calls trigger a full refresh (~4s) on every line update. 10+ updates per frame = 40s to show content. E-ink ghosting occurs because partial updates don't clear old pixels — the new system must fill with white before drawing content, and do exactly ONE `refresh(false)` per frame.

## What changes

### Core rendering pattern (replaces all `updDisp()` calls)

```cpp
// In app_modes.cpp or each draw layout function:
display->fillScreen(GxEPD_WHITE);          // Clear canvas for ghosting prevention
display->setWindow(0, 0, disp_width, disp_height);  // Set full window

// Draw everything this frame — primitives from S1
drawHeaderRow(current_mode, channel_sf_str);
drawBeaconLayout();                        // or any mode-specific layout
drawBottomStatusbar();

// ONE refresh at the end
display->refresh(false);                   // false = full update, exactly once per frame
```

For dynamic-only regions (frequent updates), use partial window:
```cpp
display->fillScreen(GxEPD_WHITE);          // White fill critical for no ghosting!
display->setPartialWindow(20, y_start, 160, region_height);  // Only update changed area

// Draw primitives in that region only
drawBeaconDistance(big_number_str, center_x, center_y);

// Partial refresh (~0.8s vs ~4s)
display->refresh(true);                    // true = partial update
```

### E-ink ghosting rules (documented at top of file)

1. **Every region must be white-filled before content draws.** If a region previously showed black pixels, drawing new content over it without white fill will leave ghost artifacts.
2. **Partial updates are safe only on regions that were already fully displayed at least once.** First pass = full refresh (primes the pixel matrix). Subsequent partial = flicker-free update of changed sub-areas.
3. **Never draw black-on-white then white-on-black in same partial region** — SSD1681 LUT doesn't handle rapid polarity inversion. Use separate partial regions for each color scheme.

### New file additions / changes

**`main/display_layout.cpp`:** Add frame engine functions:

```cpp
void beginFrameFull();   // fillScreen(WHITE) + setWindow(0,0,200,200) + set font/color/cursor defaults
void beginFramePartial(int x, int y, int w, int h);  // partial region with white fill
void endFrame(bool partial = false);               // display->refresh(partial) — ONE call per frame
void endFrameIfActive();                             // Only calls endFrame if a frame was started (no-op without beginFrame)

enum FrameType { FRAME_FULL, FRAME_PARTIAL };
extern FrameType current_frame;                    // track which frame is active
```

**`main/display_layout.h`:** Declare above functions. Add `LayoutState` field tracking dirty regions for dynamic updates:

```cpp
struct LayoutState {
    // ... existing fields ...
    bool beacon_dirty;           // true if beacon layout needs redraw
    bool ptt_dirty;              // true if PTT state block needs redraw
    unsigned long last_beacon_draw_ms;   // throttle to avoid unnecessary redraws
    unsigned long last_ptt_draw_ms;
};
```

### Migration strategy for existing callers

**No in-place `updDisp()` removal.** The layout primitives are additive — they draw directly via `display->` calls. Existing modes call their new `drawXxxLayout()` function which internally calls the primitives and the frame engine.

**After all 9 modes use the new layouts**, then remove `updDisp()` and all old rendering from `app_modes.cpp`. This story does NOT change `app_modes.cpp`'s mode logic — only adds `drawDefaultLayout()` that existing modes can call during transition.

## What stays unchanged

- All icon bitmap data
- GxEPD2 init flow in `setupDisplay()`
- Packet handling, beacon roster, peerRoster, text_inbox storage
- Button handling (AceButton), mode cycling
- BLE GATT notification sending (screen_sync.cpp/h uses the same path — it is a separate module that runs every loop iteration and does not interfere with display rendering work)
- Any non-display code in `app_modes.cpp`

## Testing & Verification

- [ ] `build_scripts\01_build_firmware.bat` compiles with zero errors
- [ ] Boot shows full refresh (~4s) — this is expected and correct
- [ ] Mode switch completes in <2s (previously 30-40s) — **critical measurement**
- [ ] Beacon mode: distance updates flicker-free in <1s via partial window (not full refresh)
- [ ] No ghosting artifacts visible on screen after switching between modes repeatedly
- [ ] Bottom status bar shows correctly in ALL modes
- [ ] Flash usage in build output: ~29-30% (frame buffer adds minimal flash)

## Risks & Mitigations

- **Partial update region must have been fully displayed first:** The full-screen boot display primes the panel. Every partial window must be within bounds that were shown at least once during init. ✓ satisfied by design
- **White fill before partial draws is mandatory:** If a pixel was black and we draw new content without clearing it, ghosting occurs. Mitigation: `fillScreen(GxEPD_WHITE)` or `fillRect(x,y,w,h,GXEPD_WHITE)` always precedes partial content draws.

## Story Index Dependencies

- **Required before:** S1 (primitives must exist)
- **Blocks:** S3-S8 (every mode's rendering depends on frame engine)
- **Independent of:** none within this epic
