# S1 — Display Layout Primitives + Infrastructure

## Status: DONE

### Build Results
- Build: **SUCCESS** (zero errors, 29% flash, 16% RAM)
- Boot animation: intact (boot_animation.cpp untouched)
- Flash impact: ~0KB added to total (stubs are no-op, primitives add minimal code)

## Context

The current `display.cpp` has no layout abstraction. Each mode draws text at hardcoded line numbers with inconsistent spacing. We need a shared set of drawing primitives that every mode layout will compose from, plus the new file structure alongside the existing display module.

## What changes

### New file: `main/display_layout.h`

Declarations for all layout primitives and per-mode draw functions:

```cpp
// ── Primitives (used by every mode) ──
void drawHeaderRow(const char* mode_name, const char* channel_sf);
void drawBottomStatusbar();
void drawBlackBoxTag(const char* text, int x, int y, int w, int h);
void drawSolidBar(int x, int y, int w, int h, uint16_t color);  // divider bars
void drawBorderedRect(int x, int y, int w, int h);               // outer box
void drawPrimaryValue(const char* text, const Adafruit_GFX_Font* font, int x, int y);
void drawSecondaryRow(const char* text, int y);
void drawMonospaceAligned(const char* text, int x, int y);

// ── Per-mode layouts (called from app_modes) ──
void drawBeaconLayout();
void drawRangeLayout();
void drawPttLayout();
void drawTxtSingleLayout();
void drawTxtInboxLayout();
void drawTstLayout();
void drawPongLayout();
void drawScanLayout();
void drawRawLayout();
void drawWpLayout();

// ── Layout state (current mode's data) ──
struct LayoutState {
    const char* mode;
    int beacon_peer_name;     // index into display buffer for beacon peer name
    double beacon_distance;
    int range_distance_m;
    bool range_sender;
    bool ptt_sending;
    bool ptt_receiving;
    int pong_state;           // 0=idle, 1=sending, 2=receive
    int pong_rtt_ms;
    int scan_progress_pct;
    char scan_current_freq[16];
    char raw_hex_line1[40];
    char raw_ascii_line[40];
    int tst_sent;
    int tst_rcvd;
    double wp_lat, wp_lon;
    float wp_alt;
    char wp_label[25];
    bool wp_broadcasting;
    uint32_t wp_bcast_remaining_s;
};
extern LayoutState layout_state;
void initLayoutState();
```

### New file: `main/display_layout.cpp`

Implementation of all primitives. Uses existing `display->` GxEPD2 calls and `FreeMonoBold9pt7b` / `FreeMonoBold12pt7b` fonts. **No frame rendering yet** — just draw-to-canvas functions that can be called in any order within a page block.

#### Key implementation details:

- **Header row (y=12):** Draw mode icon at (0, 12), then black-filled rect for mode name at x~24 with white text, then channel/SF right-aligned at far-right edge
- **Black box tag:** `fillRect(x, y, w, h, BLACK)` + `setTextColor(WHITE)` + `setFont(&FreeMonoBold9pt7b)` + `print(text)` + revert colors
- **Bottom status bar (y=168):** First fill entire 200x32 rect with BLACK. Then draw frequency (white text, x=4), time (white text, center ~95 sats (white text, ~155), battery icon (white, far-right). White-on-black everywhere in the bar
- **Solid divider:** `fillRect(x, y, w, 3, BLACK)` — 3px minimum for visual weight
- **Bordered rect:** Four `drawRect` calls (top/bottom/left/right edges) in BLACK with WHITE fill inside
- **Primary value:** Center text horizontally at given Y using `setTextSize()` or font selection. Use `FreeMonoBold12pt7b` if available, else `FreeMonoBold9pt7b`. Cursor set via `display->getCursorX()` and screen width / 2 minus text width / 2
- **Secondary row:** Left-aligned at given Y in `FreeMonoBold9pt7b` (or small monospace equivalent)

### Changes to existing files:

**`main/display.cpp`:** Keep all icon data, battery icons, GPS icons, and the existing `setupDisplay()` path. Remove line-by-line update logic (`updDisp`, `printStatusIcons`, etc.) — these will be replaced by layout calls.

**`main/display.h`:** Remove old function declarations (`updDisp`, `updModeAndChannelDisplay`, `printStatusIcons`, etc.). Add `extern LayoutState layout_state;`. Keep `setupDisplay()`, `drawIcon()`, `drawModeIcon()`, `enableBacklight()`.

## What stays unchanged (in-scope boundaries)

- All icon bitmap data (`txt_icon`, `ptt_icon`, battery icons, GPS icons, etc.) — copied verbatim
- `setupDisplay()` init flow — SPI, GxEPD2 init, backlight
- Battery monitoring (`battery.cpp`)
- GPS parsing (`gps.cpp`)  
- RTC time reading (`settings.cpp` / `rtc`)
- All LoRa packet logic in `lora.h/.cpp`, `packet.h/.cpp`
- Text inbox storage in RTC RAM (`text_inbox.cpp`) — only the *display* changes

## Implementation order within the story

1. Create `display_layout.h` with declarations + `LayoutState` struct
2. Implement each primitive function in `display_layout.cpp` (one at a time, test by build)
3. Update `display.h` to remove old declarations
4. Update `display.cpp` — keep icons/setup, strip `updDisp`/`printStatusIcons`/`updModeAndChannelDisplay`
5. Wire up: add an empty `drawDefaultLayout()` that calls `drawHeaderRow()` + `drawBottomStatusbar()` so existing modes compile

## Testing & Verification

- [ ] `build_scripts\01_build_firmware.bat` compiles with zero errors
- [ ] Boot animation still shows (existing `boot_animation.cpp` untouched)
- [ ] After boot, T-Echo screen shows header row + bottom status bar layout (manual check on device)
- [ ] Flash usage report in build output (~29-30%, minimal addition for primitives)

## What the next story builds on

S2 will wrap all primitives into a `beginFrame()`/`endFrame()` pattern with page-mode rendering and single refresh. No other story depends on S1 directly — just both need the file structure established here.
