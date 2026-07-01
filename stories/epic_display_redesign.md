# Epic — E-Paper Display Redesign (Layout Overhaul)

## Purpose

Replace the current line-by-line display rendering (`updDisp()` with per-call full refresh) with a new layout system that uses Big Card and Split Rows layouts. Fixes broken/garbage display output, eliminates ~40s-per-frame render times, prevents e-ink ghosting, and implements all 9 mode-specific layouts per the design spec.

## Problem

The current implementation:
1. Uses `updDisp()` which triggers a full refresh (~4s) on every call — 10+ calls per frame = 40s to update screen
2. Displays garbage on e-paper because partial updates without white-bg fill cause ghosting artifacts
3. Has no layout primitives — each mode writes text at hardcoded line numbers with inconsistent spacing
4. Missing WP mode from the `modes[]` array despite being documented

## Solution

New `display_layout.h/.cpp` module providing:
- Frame-based rendering: all content drawn within one `firstPage()/nextPage()` block + ONE refresh
- Layout primitives: header row, bottom status bar (solid black 32px bar), black-box tags, divider bars, primary values
- E-ink safe: white background fill before content draws; partial-window updates with white-bg for dynamic-only regions (~0.8s vs ~4s)

## Scope

| In scope | Out of scope |
|---|---|
| New display rendering engine | Packet/broadcast logic changes |
| All 9 mode layouts (BEACON, RANGE, PTT, TXT, TST, PONG, SCAN, RAW, WP) | Changes to LoRa radio config |
| WP mode integration into modes array | Companion app BLE changes |
| E-ink ghosting prevention | Audio Opus handling |
| Flash usage within target (~32%) | PlatformIO build support |

## Dependency Chain

```
S1 (primitives) [DONE] → S2 (frame engine) → S3, S4, S5, S6, S7, S8 (parallel) → S9 (WP integration)
```

Stories 3-8 are independent of each other — implement in any order after S1+S2.

## Acceptance Criteria

- [ ] Firmware builds with zero errors via `build_scripts\01_build_firmware.bat`
- [ ] All 9 modes cycleable via MODE button
- [ ] Each mode renders correctly on hardware (no garbage/ghosting)
- [ ] Mode switch completes in <2s (not 30-40s as before)
- [ ] Dynamic updates (beacon distance, PTT state) flicker-free at <1s with partial refresh
- [ ] Bottom status bar always shows frequency, time (HH:MM), sat count, battery icon
- [ ] Header row in every mode: [icon] black-box MODE_NAME right-aligned chn:X spf:N
- [ ] WP mode present in `modes[]` array and functional | **Done** |
- [ ] Flash usage ~30-32% of 768KB (verified via build output)

## Story Index

| Story | Name | Files Changed |
|---|---|---|
| S1 | Layout primitives + infrastructure | `display_layout.h/.cpp` (new), `display.cpp/h` (partial) |
| S2 | Frame engine + e-ink safe rendering | `display_layout.h/.cpp`, all callers |
| S3 | BEACON Split Rows | `display_layout.cpp` add_beacon() |
| S4 | RANGE Big Card | `display_layout.cpp` add_range() |
| S5 | PTT Big Card TX/RX block | `display_layout.cpp` add_ptt() |
| S6 | TXT single + inbox card | `display_layout.cpp` add_txt(), `text_inbox.cpp` updates |
| S7 | TST Dashboard grid | `display_layout.cpp` add_tst() |
| S8 | PONG, SCAN, RAW, WP layouts | `display_layout.cpp` add_pong(), add_scan(), add_raw(), add_wp() | **Done** |
| S9 | WP mode integration | `app_modes.h/.cpp`, `display_layout.cpp` |

## Risks

- **Flash pressure:** New layout functions + frame buffer pages may push past 32%. Mitigation: profile per-story, trim if needed.
- **GxEPD2 partial refresh limits:** SSD1681 only supports partial updates of previously-displayed regions (must be black-on-white or white-on-black). White-bg fill in partial window is critical to prevent ghosting.
- **No automated display testing:** Manual hardware verification required for every story.

## Verification Path

For each story: 1) Build `01_build_firmware.bat` → 2) Upload `02_upload_firmware.bat` (300s timeout) → 3) Verify on device → 4) Confirm flash/RAM in build output.
