# S8 — Remaining Modes: PONG, SCAN, RAW, WP Layouts

## Context

This story covers four modes that each have distinct visual requirements but share the same infrastructure (S1 primitives + S2 frame engine). Each mode is independent and can be implemented separately within this story.

**No changes to packet logic, scan algorithm, raw packet handling, or waypoint broadcast.** Pure display change for all four modes.

---

## 8a: PONG Mode (Big Card)

### Layout spec

```
y=12  [pong_icon] ┌─PING-PONG─┐  chn:A spf:7      ← header row (shared)
y=40                                                 
y=48    ┌──────────────────┐                         ← bordered state block
y=52    │                  │
y=56    │   TAP TO START   │                         ← idle state — text varies by exchange
y=60    │                  │
y=76    └──────────────────┘
y=104                                                    
y=120  Response time: 0.8s                             ← RTT (FreeMonoBold9pt7b)
y=136  Exchange #42                                    ← exchange counter
```

### States (state block text varies)

| State | Text in block |
|---|---|
| Idle (no activity) | "TAP TO START" |
| Sending (after touch, before pong) | "PING ►" |
| Receiving pong | "◉ PONG" |

### Rules

- State block is the primary element with generous padding above/below (~50% whitespace)
- RTT shown as "Response time: X.Xs" — format to one decimal place seconds
- Exchange number as "#N" — increment per ping initiated
- `pong_rtt_ms` stored in LayoutState from existing PONG receive timing

### Data sources (existing)

- `debouncedTouchPress()` — detects touch to initiate ping
- `range_consecutive_ok` / tracking used for pong exchange counter (or add new `pong_exchange_num`)

---

## 8b: SCAN Mode (Split Rows with Progress Bar)

### Layout spec

```
y=12  [scan_icon] ┌─FREQ SCAN─┐  chn:A spf:7       ← header row (shared)
y=40  ████████████████████████░░░░░░░░░░░░           ← progress bar (black fill left→right, white bg for empty)
y=42                   73%                             ← percentage right-aligned inside bar
y=68         Current: 908.25 MHz                       ← big number
y=100 ────────────────────────────                    ← solid 3px black divider bar
y=116   Q   RSSI     FREQ                            ← monospace table header
y=132   85  -42.0    903.50MHz
y=148   78  -38.5    904.00MHz
```

### Rules

- Progress bar: solid black fill for completed percentage, white background for remaining — uses `fillRect` with BLACK on left portion and WHITE on right portion
- Percentage text right-aligned inside the bar at y=42 (within white space or below bar edge)
- Current frequency: big centered number with "Current:" prefix in monospace
- 3px black divider bar separates progress/freq from results table
- Results table: monospace columns — quality, RSSI, frequency. Max show top 3 entries within available lines

### Progress bar calculation

```cpp
int total_steps = (endFreq - startFreq) / stepSize;  // e.g., (928 - 862) / 0.5 = 132 steps
int completed = scanCompletedSteps;                     // tracked in scan.cpp
int progress_pct = (completed * 100) / total_steps;
// Bar: fillRect(x, y, progress_w, bar_h, BLACK); where progress_w = progress_pct * bar_total_width / 100
```

### Data sources (existing)

- `scanning` — bool from scan.cpp
- `currentFrequency`, `sampleCount`, `numSamples` — current scan state
- `topChannels[]` — ChannelResult array with `.frequency`, `.rssi`, `.quality`
- `startFreq`, `endFreq`, `stepSize` — scan range parameters

---

## 8c: RAW Mode (Split Rows Hex Dump)

### Layout spec

```
y=12  [raw_icon] ┌─RAW PACKET─┐  chn:A spf:7        ← header row (shared)
y=40  \x48\x65\x6c\x6c\x6f\x20                            ← hex dump row (~18 bytes max)
y=56  \x57\x6f\x72\x6c\x64\x21\x0a                        ← continued hex, monospace
y=84  [ASCII]: Hello World!                               ← decoded interpretation
y=100 SNR: -9.5dB RSSI: -67dBm                            ← signal metrics
y=136 Pkt#: 47  Len: 13                                   ← packet metadata
```

### Rules

- Hex dump rows show bytes as `\xNN` format, max ~18 bytes per row (monospace)
- ASCII decode on separate line labeled `[ASCII]:` with decoded printable characters only
- Non-printable hex bytes shown in the hex row; corresponding position in ASCII row = `.` 
- Packet metadata: Pkt# and Len on same line

### Hex dump formatting

```cpp
// In handlePacket(), existing packet.raw[] and packet.rawLength
char hex_line[80];
int hi = 0;
for (int i = 0; i < min(18, packet.rawLength) && hi < 76; i++) {
    hi += snprintf(hex_line + hi, 5, "\\x%02X", packet.raw[i]);
}
hex_line[hi] = '\0';
drawSecondaryRow(hex_line, 40);

// ASCII decode line
char ascii_line[80];
int ai = 0;
for (int i = 0; i < min(18, packet.rawLength) && ai < 76; i++) {
    ascii_line[ai++] = isprint(packet.raw[i]) ? packet.raw[i] : '.';
}
ascii_line[ai] = '\0';
drawSecondaryRow(ascii_line, 56);  // second hex row at y=56

// ASCII label line at y=84
char ascii_label[80];
snprintf(ascii_label, sizeof(ascii_label), "[ASCII]: %s", ascii_line);
drawSecondaryRow(ascii_label, 84);
```

### Data sources (existing)

- `packet.raw[]` and `packet.rawLength` — raw received bytes
- `radio->getSNR()`, `radio->getRSSI()` — signal metrics

---

## 8d: WP Mode (Split Rows with Centered Label)

### Layout spec

```
y=12  [wp_icon] ┌─WAYPOINT─┐  chn:A spf:7             ← header row (shared)
y=44    "Summit Peak"                                   ← big centered text in quotes, FreeMonoBold12pt7b
y=68  34.052237, -118.257397                           ← GPS coords (monospace, left-aligned)
y=84  Alt: 892m                                         ← altitude
y=124 ┌─BROADCASTING (42s)─┐                          ← countdown tag only when broadcasting
y=156 Touch to save & broadcast                         ← prompt
```

### Rules

- Waypoint label is the primary element — big centered text in double quotes
- GPS coords and altitude shown as secondary info below
- Broadcast countdown: black-box tag with remaining seconds (only visible when `wpBroadcastActive`)
- Prompt at bottom tells user what to do next

### WP mode addition (missing from modes array)

This story adds "WP" to the `modes[]` array in `app_modes.cpp`:
```cpp
const char* modes[] = { "BEACON","RAW","TXT", "RANGE", "TST","PONG","SCAN","PTT", "WP" };
// Update numModes accordingly (now 9)
```

And wire up WP mode initialization in `updMode()`:
```cpp
if(current_mode == "WP") {
    // Init waypoint display state
}
```

### Data sources (existing)

- `wpStoredWaypoints[]` / `wpStoredCount` — stored waypoints from RTC RAM
- `wpBroadcastActive` / `wpBroadcastStartMs` — broadcast timer state
- `gps_latitude`, `gps_longitude`, `altitude` — GPS data for new waypoint

## Testing & Verification (all four modes)

- [ ] `build_scripts\01_build_firmware.bat` compiles with zero errors
- [ ] **PONG**: State block shows correct text per state, RTT and exchange number below
- [ ] **SCAN**: Progress bar animates during scan, results table shows quality/RSSI/freq
- [ ] **RAW**: Hex dump rows formatted correctly, ASCII decode line matches
- [ ] **WP** (new mode): Mode selectable via MODE button, label shown big and centered

## What stays unchanged

- Scan algorithm in `scan.cpp` (frequency stepping, RSSI/SNR sampling)
- Raw packet content handling in `handlePacket()` 
- Waypoint broadcast logic (`startWaypointBroadcast()`, WP packet format)
- Ping-pong state machine in `handlePacket()` PONG section

## Story Index Dependencies

- **Required before:** S1 (primitives), S2 (frame engine)
- **Blocks:** none within this epic
