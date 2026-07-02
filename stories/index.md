# Story Index — E-Paper Display Redesign Epic

## All Stories

| # | Story | Files Changed | Flash Impact | Status |
|---|---|---|---|---|
| S1 | Layout primitives + infrastructure | `display_layout.h/.cpp` (new), `display.h` (partial) | ~0KB | **Done** |
| S2 | Frame engine + e-ink safe rendering | `display_layout.h/.cpp`, all callers | ~1KB | Not started |
| S3 | BEACON Split Rows layout | `display_layout.cpp` add_beacon() | ~0.5KB | Not started |
| S4 | RANGE Big Card layout | `display_layout.cpp` add_range() | ~0.5KB | Not started |
| S5 | PTT Big Card TX/RX block | `display_layout.cpp` add_ptt() | ~0.5KB | **Done** |
| S6 | TXT single + inbox card | `display_layout.cpp` add_txt(), `text_inbox.cpp` updates | ~1KB | Not started |
| S7 | TST Dashboard grid | `display_layout.cpp` add_tst() | ~0.5KB | Not started |
| S8 | PONG, SCAN, RAW, WP layouts | `display_layout.cpp` 4 new functions | ~2KB | **Done** |
| S9 | WP mode integration | `app_modes.h/.cpp`, `display_layout.cpp` | ~1KB | Not started |

**Total estimated flash addition: ~8-10KB (from ~29% → ~31-32%)**

## Dependency Chain

```
S1 ──→ S2 ──→ S3, S4, S5, S6, S7, S8 ──→ S9
                              │                              │
                              └── 6a (PONG)                  └── 8d (WP layout)
                              8b (SCAN)                         in S9 (modes[] wire-up)
                              8c (RAW)
```

S3-S8 are independent of each other — implement in any order.

## Epic Document

See `../epic_display_redesign.md` for full epic context, acceptance criteria, risks, and verification path.
