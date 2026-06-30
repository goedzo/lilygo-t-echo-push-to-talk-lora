# S4 — RANGE Mode Layout (Big Card)

## Context

RANGE mode measures distance between sender/receiver devices using GPS coordinates. Current display shows role, GPS coords, range data on hardcoded lines with inconsistent formatting. This story replaces it with the Big Card layout: massive centered distance as primary element.

**No changes to `sendRangeMessage()`, range calculation, or packet format.** Pure display change.

## Layout spec (exact pixel positions)

```
y=12  [range_icon] ┌─CURRENT DISTANCE─┐  chn:A spf:7   ← header row (shared)
y=40                                                 
y=48                        142m                         ← MASSIVE centered distance — biggest text on screen (FreeMonoBold12pt7b or display->setTextSize(3))
y=96                                                     
y=108       ┌─SENDER ◄──► RECEIVER─┐                   ← black-box tag, left-aligned
y=124         MAX: 523m  STABLE: 140m                  ← secondary row (FreeMonoBold9pt7b)
y=140           PLoss: 2/47 ok                          ← loss line
y=156       34.0522, -118.2574                          ← GPS coords if available (monospace)
```

### Rules

- Distance number is the BIGGEST text on screen — use `FreeMonoBold12pt7b` at largest available size, centered horizontally and vertically in the middle region
- Role tag: white-on-black filled rect with "SENDER ◄──► RECEIVER" or "◄──► RECEIVER / SENDER ◄──► RECEIVER" depending on which role is active (sender gets highlight)
- No "Distance:" prefix — the context (RANGE mode + big number) makes it implicit
- GPS coords shown only if `gps_status == GPS_LOC`, otherwise show nothing (no "No GPS Fx" message in this layout)

## Data sources (existing, unchanged)

- `range_max_dist` — maximum distance seen so far
- `range_stable_dist` — stable distance after consecutive OK packets
- `range_total_pckt_loss` / `range_consecutive_ok` — packet loss tracking  
- `range_role_sender` — true = sender, false = receiver
- `gps_latitude`, `gps_longitude`, `gps_status` — GPS data

## Implementation

In `display_layout.cpp`:

```cpp
void drawRangeLayout() {
    // Big centered distance at y=48
    char dist_buf[20];
    if (range_max_dist > 0) {
        snprintf(dist_buf, sizeof(dist_buf), "%.0fm", range_max_dist);
    } else {
        snprintf(dist_buf, sizeof(dist_buf), "???");
    }
    drawPrimaryValue(dist_buf, &FreeMonoBold12pt7b, 100, 48);

    // Role tag at y=108 (black-box)
    char role_tag[32];
    if (range_role_sender) {
        snprintf(role_tag, sizeof(role_tag), "SENDER ◄──► RECEIVER");
    } else {
        snprintf(role_tag, sizeof(role_tag), "◄──► RECEIVER");
    }
    int tag_w = strlen(role_tag) * 7 + 12;  // approximate width estimate
    drawBlackBoxTag(role_tag, x_start, 108, tag_w, 24);

    // Metrics at y=124 (secondary row)
    char metric_buf[50];
    snprintf(metric_buf, sizeof(metric_buf), "MAX: %.0fm STABLE: %.0fm", 
             range_max_dist, range_stable_dist);
    drawSecondaryRow(metric_buf, 124);

    // Loss at y=140
    char loss_buf[40];
    snprintf(loss_buf, sizeof(loss_buf), "PLoss: %d/%d ok", 
             range_total_pckt_loss, range_consecutive_ok);
    drawSecondaryRow(loss_buf, 140);

    // GPS coords at y=156 if available
    if (gps_status == GPS_LOC) {
        char gps_buf[32];
        snprintf(gps_buf, sizeof(gps_buf), "%.6f, %.6f", gps_latitude, gps_longitude);
        drawMonospaceAligned(gps_buf, 0, 156);
    }

    // Clear remaining lines
}
```

## Testing & Verification

- [ ] `build_scripts\01_build_firmware.bat` compiles with zero errors
- [ ] On device: massive centered distance number clearly visible as primary element
- [ ] Tapping switches role between Sender/Receiver with display update
- [ ] Distance updates (range_max_dist, range_stable_dist) reflect correctly
- [ ] GPS coords shown when fixed, hidden when not
- [ ] Flicker-free range data updates via partial window of middle region

## What stays unchanged

- `sendRangeMessage()` — auto-sends every 5s when sender
- Range calculation via `TinyGPSPlus::distanceBetween()`
- Packet format: `RN{channel}test{n}`
- Home location setup (`range_home_lat`, `range_home_long`)

## Story Index Dependencies

- **Required before:** S1 (primitives), S2 (frame engine)
- **Blocks:** none (independent of other mode stories)
