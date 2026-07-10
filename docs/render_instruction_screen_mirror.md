# Render Instruction Screen Mirror — Complete Design

## 1. Functional Goal (What)

Currently the app mirrors the device screen by sending structured per-mode data fields (`M:mode`, `C:content_fields`, `S:freq`, `T:time`, `G:sats`, `B:batt%`) that the app re-interprets into HTML/CSS. This means the app has its own independent rendering of every mode — duplicated layout logic, prone to drifting from what the device actually shows (font differences, truncation, alignment shifts).

**Replace structured fields with render instructions.** The device sends the exact primitive drawing commands (`fillRect`, `print`, `drawIcon`) that it executed to produce the screen. The app re-executes them in order — same positions, same fonts, same colors — producing a pixel-identical mirror on `<canvas>`. Zero ambiguity, zero duplicated layout logic in the app, immune to font/rendering drift because both sides use the same primitives in the same order.

When to send:
- **Proactively**: Every time the screen actually updates (state change, new message, scan progress). Only dirty frames are sent — stable screens generate zero traffic.
- **On-demand**: When the app sends "GETSCREEN", the device always responds with the current frame (forced mode, ignores dirty flag).

---

## 2. Technical Design (How)

### Render Pass Capture

After `renderPageLoop()` finishes drawing in `display_layout.cpp`, replay the draw operations that were executed. Instead of trying to capture the GxEPD2 internal SRAM buffer (`_buffer`) — which requires complex memory access, BLE MTU negotiation, and RLE compression — we instrument the layout functions to emit render instructions during the normal draw pass.

Two approaches for capturing instructions:

**Approach A (recommended): Instruction emitter in `renderPageLoop()`**
Modify `renderPageLoop()` in `display_layout.cpp` to wrap each call to `display->fillRect()`, `display->print()`, etc. with an instruction-emitting decorator. A simple callback/lambda records every primitive call with its parameters into a ring buffer (`_instr_ring[128 entries]`). After the page loop completes, emit those instructions as the frame payload.

**Approach B: Layout-level emission**
Each `drawXxxLayout()` function calls an existing `emitInstruction()` helper for each draw primitive it executes. This is more explicit and easier to maintain but requires touching every layout function.

### Instruction Format

```
LINE:F|R:{mode}
F0:fillRect 12,32,176,80,WHITE
T1:print "CN_Bob"    cursor=16,36   font=9pt   color=BLACK
B2:battery_icon       pos=184,170    bg=BLACK
T3:print "433.50MHz"  cursor=4,188   font=9pt   color=WHITE
F4:fillRect 0,168,200,32,BLACK
~~
```

Primitive types:
| Prefix | Primitive | Payload Format |
|--------|-----------|---------------|
| `F:` | fillRect | x,y,w,h,COLOR |
| `D:` | drawRect (border) | x,y,w,h,COLOR |
| `T:` | print text | "text" cursor=x,y font=SIZE color=COLOR |
| `B:` | battery_icon | pos=x,y bg_color |
| `I:` | mode/status icon | pos=x,y icon_id bg_color icon_color |
| `P:` | fillCircle/drawLine | x,y,radius/color or x1,y1,x2,y2,color |

Colors encoded as: WHITE=0, BLACK=1 (single bit). This maps 1:1 to the e-paper display's 1-bit nature.

### Compression Characteristics

E-paper screens are ~70-90% solid background rectangles. A typical frame's instruction list is heavily dominated by fillRect background blocks that can be batched:

**Frame compression**: Group consecutive fillRect calls on the same Y coordinate with adjacent x positions into single "row-fill" instructions:
```
ROW_FILL 12,32..188:WHITE   (replaces 6 individual fillRect calls → 1 instruction)
```

After row-batching, most screens send **~15-25 compact instructions** (~200-400 bytes per frame). This is:
- Comparable to or even **smaller than** the current structured-field payload (~230 bytes)
- Far smaller than bitmap RLE (~600-800 bytes after compression)
- No BLE MTU negotiation needed — each frame fits in a single notification chunk

### Frame Dirty Flag + Sync Flow

```
Device side                          App side
─────────────                        ──────────
renderPageLoop() completes           [stable, no traffic]
  → emit instructions              GETSCREEN arrives?
  → row-batch + compare with        Yes → sendFrameForced()
    previous frame (memcmp)         No → wait for next change
  → if dirty+connected+room:
       queue LINE:F notification
```

The dirty flag works the same as `screen_sync.cpp`'s `markScreenDirty()` pattern — compare emitted instruction list against the previous frame. If identical, zero traffic. Only the next screen change re-dirties it.

---

## 3. Code Changes (What to Make/Update)

### New Files

#### `main/screen_bitmap.h`

```cpp
#ifndef SCREEN_BITMAP_H
#define SCREEN_BITMAP_H

// Instruction ring buffer: max 128 instructions per frame
#define MAX_INSTRS    128
#define INSTR_BUF_SZ  4096  // room for text in instructions

// Primitive types
enum InstrType {
    INSTR_FILLCRECT = 0,   // fillRect x,y,w,h,color
    INSTR_DRAWRECT  = 1,   // drawRect border x,y,w,h,color  
    INSTR_TEXT      = 2,   // print "text" cursor=x,y font=color
    ICON_BATTERY  = 3,     // battery_icon pos=bg
    ICON_MODE       = 4,   // mode/status icon
    ICON_FILL       = 5,   // row-fill batching (internal)
    INSTR_LINE      = 6,   // drawLine x1,y1,x2,y2,color
    INSTR_CIRCLE    = 7,   // fillCircle/drawCircle x,y,r,color
};

// Emit a render instruction during the draw pass (called by layout functions)
void emitInstr(InstrType type, const uint8_t* params, uint8_t len);

// Start/commit a new frame — called from renderPageLoop() after drawing completes
void beginFrameCapture();      // clear instruction buffer
void endFrameCapture();        // row-batch + compare with previous frame + send

// Force send regardless of dirty flag (for GETSCREEN responses)
void sendFrameForced();

#endif
```

#### `main/screen_bitmap.cpp`

Contains:
- **Instruction ring**: `static struct {type; uint8_t params[32]; uint8_t len;} instrs[MAX_INSTRS]` in SRAM (~4.5KB worst case)
- **Row-batching**: After `endFrameCapture()`, scan consecutive fillRect calls on same Y, merge adjacent ranges into ROW_FILL instructions (reduces typical frame from 15-25 instructions to ~10-15)
- **Dirty detection**: Compare batched instruction list against previous frame via byte-level memcmp (~2μs for 4KB on Cortex-M4 @ 168MHz)
- **Notification transport**: Batch all instructions into a single `LINE:F|R:{mode}\n...~~` payload. Payload size typically ~250 bytes, fits within SYNC_MAX_PAYLOAD. Use existing `sendNotificationToApp()` from `screen_sync.cpp`.

### Modified Files

#### `main/display_layout.cpp`

Modify `renderPageLoop()` (line 179): wrap the page loop with instruction capture:

```cpp
#include "screen_bitmap.h"

static void renderPageLoop(drawFn drawContent, bool use_full_refresh) {
    beginFrameCapture();     // ← NEW: start recording draw primitives
    
    display->setPartialWindow(0, 12, disp_width, 184);
    epdSetGRAMWindow(0, 12, disp_width, 184);

    display->firstPage();
    do {
        display->fillScreen(GxEPD_WHITE);
        drawContent();
    } while (display->nextPage());

    triggerEpdRefresh(false);
    
    endFrameCapture();     // ← NEW: row-batch + compare + send
}
```

Also modify each layout function to emit instructions for its draw calls. Example for `drawDefaultLayout()`:

```cpp
// Inside the drawContent lambda:
auto fb = [](int x, int y, int w, int h, uint16_t color) {
    static uint8_t params[5];
    params[0] = x; params[1] = y; params[2] = w; params[3] = h;
    params[4] = (color == GxEPD_BLACK) ? 1 : 0;
    emitInstr(INSTR_FILLCRECT, params, 5);
};

fb(12, disp_top_margin, 100, 16, GxEPD_BLACK);  // header background
// ... etc for each draw call in the layout
```

A more practical approach: create a thin wrapper class `DrawLogger` that intercepts all display calls via composition (wrap `GxEPD2_BW*` and proxy to both the real display AND instruction emitter). This avoids touching every existing layout function individually — just wrap the `display` global during renderPageLoop().

#### `main/main.ino` or `main/app_modes.cpp`

Add call to process bitmap frames in the main loop (or wherever `sendScreenSyncIfDirty()` is called from `screen_sync.cpp`). The dirty frame sends are throttled internally (~1/sec max), so calling every loop iteration is safe.

#### `cordova_app/PTTLora/www/index.html`

Replace the existing `<div class="screen-content-area" id="screenContentArea">` with a canvas element:

```html
<div class="screen-content-area" id="screenContentArea">
    <canvas id="screenRender" width="200" height="200" 
            style="width:300px;height:300px;image-rendering:pixelated;"></canvas>
</div>
```

#### `cordova_app/PTTLora/www/css/index.css`

Style the canvas for crisp rendering:

```css
#screenRender {
    image-rendering: pixelated;
    image-rendering: -moz-crisp-edges;
    image-rendering: crisp-edges;
    background: #fff;
}
```

#### `cordova_app/PTTLora/www/js/index.js`

In the notification handler, add a new case for `LINE:F` alongside the existing `LINE:S` and `LINE:NOTIF` handlers. When `lineMatch[1] === 'F'`, dispatch to render-instruction handler. Also replace `parseScreenMirrorFields()` and `renderScreenMirror()` with instruction-specific functions.

```javascript
// Add to notification processing in index.js (before LINE:S handler):

if (lineMatch && lineMatch[1] === 'F') {
    app._handleRenderFrame(message);
    return;
}

// Render frame state:
_renderFrameState: { active: false, canvas: null, ctx: null },

// Handle "LINE:F|R:{mode}\nPRIM...\n~~" payload
_handleRenderFrame: function(msg) {
    // Strip LINE:F| prefix and ~~ terminator
    var body = msg.slice(6, -2);  // "R:{mode}\nF0:...T1:...\n..."
    
    var lines = body.split('\n');
    var modeLine = lines[0];  // "R:BEACON"
    var mode = modeLine.slice(2);
    
    var canvas = document.getElementById('screenRender');
    if (!canvas) return;
    
    var ctx = canvas.getContext('2d');
    ctx.fillStyle = '#fff';
    ctx.fillRect(0, 0, 200, 200);  // white background
    
    // Execute primitives in order
    for (var i = 1; i < lines.length; i++) {
        var line = lines[i];
        if (!line) continue;
        
        var primType = line[0];   // F, D, T, B, I, P
        var primId   = line[2];   // instruction index
        var primData = line.slice(4);
        
        switch (primType) {
            case 'F': this._execFill(ctx, primData, '#000', '#fff'); break;
            case 'D': this._execDrawRect(ctx, primData); break;
            case 'T': this._execText(ctx, primData); break;
            case 'B': this._execBatIcon(ctx, primData); break;
            case 'I': this._execModeIcon(ctx, primData); break;
        }
    }
    
    // Update mode badge
    document.getElementById('screenModeBadge').textContent = mode;
},

// Primitive executors:
_execFill: function(ctx, data, blackHex, whiteHex) {
    var parts = data.split(',');
    var x = +parts[0], y = +parts[1], w = +parts[2], h = +parts[3];
    var color = +parts[4] ? blackHex : whiteHex;
    ctx.fillStyle = color;
    ctx.fillRect(x, y, w, h);
},

_execText: function(ctx, data) {
    // "CN_Bob" cursor=16,36 font=9pt color=BLACK
    var quoteIdx = data.indexOf('"');
    var text = data.slice(quoteIdx + 1, data.indexOf('"', quoteIdx + 1));
    var cursorPart = data.split('cursor=')[1]?.split(' ')[0] || '0,0';
    var fontPart = data.split('font=')[1]?.split(' ')[0] || '9pt';
    var colorPart = data.split('color=')[1] || 'BLACK';
    var color = colorPart === 'WHITE' ? '#fff' : '#000';
    
    ctx.fillStyle = color;
    ctx.font = fontPart.replace('pt', 'px') + ' FreeMonoBold';
    var parts = cursorPart.split(',');
    ctx.fillText(text, +parts[0], +parts[1]);
},

_execBatIcon: function(ctx, data) {
    // Battery icon at pos=x,y on a white canvas draws the battery shape
    this._drawBatIcon(ctx, +data.split(',')[0], 170);
},

_execModeIcon: function(ctx, data) {
    // Mode icon: render one of the 16 predefined icons at given position
    var params = data.split(',');
    var iconId = parseInt(params[2]);  // icon identifier from mode
    this._drawModeIcon(ctx, +params[0], +params[1], iconId);
},
```

The existing `parseScreenMirrorFields()` and `renderScreenMirror()` become dead code (or can be removed entirely if no other callers exist). The render-instruction handler takes priority — `LINE:F` handler runs before the `LINE:S` handler in the message dispatch chain.

---

## 4. Payload Size Comparison

| Approach | Typical Frame | Max Frame | BLE MTU Needed |
|----------|--------------|-----------|----------------|
| Current structured fields (LINE:S) | ~150-230B | ~230B (SYNC_MAX_PAYLOAD) | None (fits existing) |
| Bitmap RLE (original design) | ~600-800B | ~1500B | **Required** (247-byte MTU) |
| **Render instructions (this design)** | **~200-400B** | **~500B** | **None** (fits existing SYNC_MAX_PAYLOAD) |

The render-instruction approach eliminates the need for BLE MTU negotiation entirely because it fits within the existing 230-byte+SYNC_MAX_PAYLOAD notification channel, same as the current structured-field approach.

---

## 5. Architecture Decision Rationale

This design replaced the original bitmap-pixel-mirror approach after analyzing three constraints:

1. **GxEPD2 `_buffer` is private** (GxEPD2_BW.h:751) — accessing it requires `reinterpret_cast` or copy-during-render, neither of which survives library updates cleanly
2. **BLE MTU negotiation is a blocking dependency** — default ATT_MTU = 23 bytes → 17 byte payload per notification means ~295 notifications per frame. Without negotiated MTU (241 bytes payload → ~21 notifications), the feature doesn't work on all phones and blocks all other BLE operations for nearly 10 seconds
3. **The device already executes deterministic render primitives** — every layout function in `display_layout.cpp` is a sequence of `fillRect()`, `print()`, `drawIcon()` calls with fixed positions. Sending these instructions instead of pixel data gives identical visual output without bandwidth blowup

The key insight: users don't want a screen mirror — they want assurance that what they see on one screen matches the other. Render instructions provide pixel-identical sync because both sides use the same primitives in the same order at the same positions, while staying within existing BLE constraints (no MTU negotiation needed).

---

## 6. Implementation Order

1. **`screen_bitmap.h` / `screen_bitmap.cpp`** — instruction emitter ring buffer + row-batching + dirty detection + notification transport
2. **Modify `renderPageLoop()` in `display_layout.cpp`** — wrap with capture begin/end, optionally add `DrawLogger` proxy around `display` to intercept all draw calls
3. **App-side: canvas renderer in `index.js`** — parse `LINE:F|R:{mode}\n...~~`, execute primitives on `<canvas>`
4. **App-side: CSS for canvas element in `index.html` / `index.css`**
5. **Deprecate/remove structured-field handler (LINE:S)** — new render-instruction path replaces it

---

## 7. Failure Modes and Mitigations

| Failure Mode | Impact | Mitigation |
|-------------|--------|------------|
| Instruction buffer overflow (>128 primitives) | Frame truncated, incomplete mirror | Use `ROW_FILL` batching to reduce count; cap at 128 and fall back to structured fields (LINE:S) with warning |
| BLE queue full during active LoRa transfer | Frame drops silently | Throttle to 1/sec max (same as existing screen_sync.cpp); drop middle frames when `getPendingNotificationCount() > 8` |
| App canvas renderer diverges from device rendering | Visual drift over time | Use same font names/sizes/positions as device; test against known layouts during development |
| New layout mode missing instruction emitter calls | Silent partial mirror | Add build-time check: every `display->fillRect/print/drawIcon` call in `drawXxxLayout()` must have a corresponding `emitInstr()` call |
