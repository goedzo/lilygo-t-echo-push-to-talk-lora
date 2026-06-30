# S3 — BEACON Mode Layout (Split Rows)

## Context

BEACON mode displays peer roster data from the `peerRoster[]` array in RTC RAM. The current rendering uses hardcoded line numbers with inconsistent formatting. This story replaces it with the Split Rows layout: closest peer name + big distance as primary element, roster below.

**No changes to packet parsing, beaconAddOrUpdate(), or peerRoster data structures.** Pure display change.

## Layout spec (exact pixel positions)

```
y=12  [beacon_icon] ┌─CLOSEST PEER─┐  chn:A spf:7       ← header row (shared)
y=28                                                    ← spacer (16px breathing room)
y=44           Alice                                   ← peer name, centered, FreeMonoBold9pt7b
y=60                     247m                            ← distance, FREEmonoBold12pt7b, HUGE, centered
y=108                                                   ← spacer (32px breathing room — distance IS the element)
y=124          ┌─PEER ROSTER─┐                          ← black-box tag, left-aligned
y=140    Alice  247m  85%                               ← roster row (monospace aligned)
y=156    Bob     1.2km -%
y=172    Carol    43m 92%
```

### Rules

- Distance is the primary visual element — takes ~40% of screen height with breathing room above and below
- Roster entries: `[name] [distance] [battery%]` — monospace aligned (name left, distance center-aligned in ~6ch slot, battery right-aligned)
- No line separators between roster entries — spacing comes from font height
- Format: `<name><pad><dist> <batt>` where name uses FreeMonoBold9pt7b (or proportionally rendered), distance uses monospace rendering via FreeMonoBold12pt7b if >500m else FreeMonoBold9pt7b, battery as `%` suffix
- Distances: `<1000m → " 247m"`, `>=1000m → " 1.2km"`
- Roster max entries: fill remaining lines (y=140 through y=168 = 2 lines for current 200px screen)

## New primitive needed (add to S1 primitives if not covered)

```cpp
// In display_layout.cpp: draw roster entries starting at given Y position
// roster_rows[] = array of beaconRosterEntry pointers, count = number of entries
void drawPeerRoster(int start_y, BeaconEntry* entries[], int count);
```

## Data sources (existing, unchanged)

- `peerRoster[]` — external array with `.callSign[16]`, `.deviceId[String]`, `.distanceM[int]`, `.battery[int]`
- `peerRosterCount` — total peers in roster
- `beacon_display_name` / `beacon_display_dist` — closest peer tracking (already exists)

## Implementation

In `display_layout.cpp`:

```cpp
void drawBeaconLayout() {
    // Find closest peer with valid distance
    int bestIdx = -1;
    double bestDist = -1;
    for (int i = 0; i < peerRosterCount; i++) {
        if (peerRoster[i].distanceM > 0 && peerRoster[i].distanceM < 50000) {
            if (bestDist < 0 || peerRoster[i].distanceM < bestDist) {
                bestDist = peerRoster[i].distanceM;
                bestIdx = i;
            }
        }
    }

    // Peer name at y=44 (centered)
    const char* name = (bestIdx >= 0) 
        ? (peerRoster[bestIdx].callSign[0] != '\0' ? peerRoster[bestIdx].callSign : peerRoster[bestIdx].deviceId.c_str())
        : "No peer loc";
    
    // Distance at y=60 (centered, huge) — this is the visual anchor
    
    // Roster header tag at y=124
    // Roster rows starting at y=140
    
    // Clear remaining lines below roster
}
```

## Testing & Verification

- [ ] `build_scripts\01_build_firmware.bat` compiles with zero errors
- [ ] On device: two T-Echos within range show closest peer name + large centered distance (247m example)
- [ ] Distance text is clearly the biggest element on screen
- [ ] Roster entries below are monospace-aligned, readable
- [ ] No roster overflow beyond available lines
- [ ] Flicker-free when beacon packet arrives (partial update of affected region only)
- [ ] Flash usage: ~0.5KB addition for beacon layout code

## What stays unchanged

- `beaconAddOrUpdate()` in packet handling
- Peer roster data structure (`peerRoster[]`, `peerRosterCount`)
- Beacon packet format (`B{id}~CN{callSign}~GP{lat},{lon}~BT{pct}`)
- Distance calculation via `TinyGPSPlus::distanceBetween()`

## Story Index Dependencies

- **Required before:** S1 (primitives), S2 (frame engine)
- **Blocks:** none (independent of other mode stories)
