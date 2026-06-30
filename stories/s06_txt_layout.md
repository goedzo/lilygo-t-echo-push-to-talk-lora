# S6 — TXT Mode Layout (Single-Message + Inbox Card)

## Context

TXT mode displays text messages from the inbox stored in RTC RAM. Current display uses hardcoded lines with "TXT Inbox" / "Inbox:" headers and scrollable message pages. This story replaces it with two distinct layouts: single-message view (one prominent row) and inbox card view (outer-bounded card containing all visible messages).

**No changes to `inboxStore()`, `inboxDisplayPage()`, or RTC RAM inbox storage.** Only the E-Paper rendering changes.

## Layout 1: Single-Message View

```
y=12  [txt_icon] ┌─INBOX─┐  chn:A spf:7     ← header row (shared)
y=40  [Alec]> Hello there! from the trail     ← one prominent row — this IS the element
y=68                                                  
y=84  gps: 34.05, -118.25                     ← GPS coordinates (monospace)
y=100 2026-06-28 14:32                        ← timestamp (HH:MM format per spec)
y=140 MODE=tap for inbox                       ← toggle hint at bottom
```

### Rules

- Message body is the primary element — one row with sender in brackets `[Alec]>` + message text
- GPS coordinates on separate row if available from packet metadata
- Timestamp shows only HH:MM (not HH:MM:SS) to match design spec for "time center"
- Toggle hint at bottom tells user how to switch views

## Layout 2: Inbox Card View

```
y=12  [txt_icon] ┌─INBOX─┐  chn:A spf:7      ← header row (shared)
y=28  ────────────────────────────              ← solid black bar (page header, white text on top)
y=30                P 1/3 (5 msgs)             ← page info in white text on black bar
y=48  ┌────────────────────────────┐           ← outer bordered rect (card boundary!)
y=52  │ [Alec]> Hello!              │          ← card contents: all visible messages
y=68  │ [Bob]> Trail is clear       │
y=84  │ [Carol]> On my way          │
y=100 │ [Dave]> Copy that           │
y=116 │ [Eve]> Roger                │
y=132 └────────────────────────────┘
y=160 MODE=tap for reader             ← toggle hint
```

### Rules

- **Outer black box contains the entire inbox page** — without it you cannot tell where the list ends
- Page header: solid black bar with white text showing page number and total messages (P 1/3 (5 msgs))
- Max 5 messages per page (to fit within card boundary)
- Card content lines: `[sender]> message` format — no separators between entries, spacing from font height only
- Sender ID displayed in brackets (from existing inbox `sender[4]` field)

## Data sources (existing, unchanged)

- `inboxCount()` — total messages stored in RTC RAM
- `inboxShowLatest()` — show latest message preview (exists already, format updated)
- `txtInboxScrollPage` — current scroll page index
- `txtShowInbox` — true = inbox view, false = single-message view
- Existing sender/timestamp from `text_inbox.cpp` inbox entries

## Implementation

In `display_layout.cpp`:

```cpp
void drawTxtSingleLayout() {
    // Message body at y=40 — one prominent row
    uint8_t msg_len = 0, sender[4] = {0};
    bool truncated = false;
    
    const char* payload = inboxGet(inboxCount() - 1, msg_len, truncated, sender);
    if (payload && msg_len > 0) {
        char msg_buf[80];
        int di = 0;
        // Sender in brackets
        msg_buf[di++] = '[';
        for (int i = 0; i < 4 && i < (sender[3] >> 4); i++) {
            msg_buf[di++] = sender[i] & 0xFF;
        }
        msg_buf[di++] = ']';
        msg_buf[di++] = '>';
        
        // Message text
        int remaining = 76 - di;
        for (int i = 0; i < msg_len && i < remaining && di < 78; i++) {
            char c = payload[i];
            msg_buf[di++] = isprint((unsigned char)c) ? c : '.';
        }
        msg_buf[di] = '\0';
        
        drawSecondaryRow(msg_buf, 40);  // This row IS the element — large enough
    }

    // GPS at y=84 (if available from last message metadata)
    if (last_msg_gps_available) {
        char gps_buf[32];
        snprintf(gps_buf, sizeof(gps_buf), "gps: %.2f, %.2f", last_msg_lat, last_msg_lon);
        drawSecondaryRow(gps_buf, 84);
    }

    // Timestamp at y=100 (HH:MM only)
    if (last_msg_datetime_available) {
        char dt_buf[9];  // "HH:MM" + null
        snprintf(dt_buf, sizeof(dt_buf), "%s-%s %s:%s", 
                 last_msg_year, last_msg_month, last_msg_hour, last_msg_min);
        drawSecondaryRow(dt_buf, 100);
    }

    // Toggle hint at y=140
    drawSecondaryRow("MODE=tap for inbox", 140);

    // Clear remaining lines
}

void drawTxtInboxLayout() {
    int total_msgs = inboxCount();
    if (total_msgs == 0) {
        drawSecondaryRow("No messages yet", 80);
        return;
    }

    // Page header bar at y=28 — solid black with white text
    int hdr_y = 28, hdr_h = 16;
    display->fillRect(20, hdr_y, 160, hdr_h, GxEPD_BLACK);  // x=20 to clear icon area gap
    
    char page_info[32];
    int lines_per_page = 5;
    int total_pages = (total_msgs + lines_per_page - 1) / lines_per_page;
    snprintf(page_info, sizeof(page_info), "P %d/%d (%d msgs)", 
             txtInboxScrollPage + 1, total_pages, total_msgs);
    
    display->setCursor(60, hdr_y + 4);
    display->fillRect(20, hdr_y, 160, hdr_h, GxEPD_BLACK);
    display->setTextColor(GxEPD_WHITE);
    display->setFont(&FreeMonoBold9pt7b);
    display->print(page_info);
    display->setTextColor(GxEPD_BLACK);  // revert for content below

    // Card outer border at y=48
    int card_x = 20, card_y = 48, card_w = 160, card_h = 96;  // fits 5 messages + padding
    drawBorderedRect(card_x, card_y, card_w, card_h);

    // Card contents: up to 5 messages from current page
    for (int i = 0; i < lines_per_page; i++) {
        int msg_idx = txtInboxScrollPage * lines_per_page + i;
        if (msg_idx >= total_msgs) break;
        
        uint8_t ml = 0, sender[4] = {0};
        const char* payload = inboxGet(msg_idx, ml, false, sender);
        if (!payload || ml == 0) continue;
        
        char row_buf[76];
        int di = 0;
        msg_buf[di++] = '[';
        for (int s = 0; s < 4 && s < (sender[3] >> 4); s++) {
            msg_buf[di++] = sender[s] & 0xFF;
        }
        if (di < 75) msg_buf[di++] = ']';
        if (di < 75) msg_buf[di++] = '>';
        
        int remaining = 73 - di;
        for (int p = 0; p < ml && p < remaining && di < 74; p++) {
            char c = payload[p];
            row_buf[di++] = isprint((unsigned char)c) ? c : '.';
        }
        row_buf[di] = '\0';
        
        int row_y = card_y + 8 + (i * 16);  // 8px padding inside card, then line height
        drawSecondaryRow(row_buf, row_y);
    }

    // Toggle hint at y=160
    drawSecondaryRow("MODE=tap for reader", 160);
}
```

## Testing & Verification

- [ ] `build_scripts\01_build_firmware.bat` compiles with zero errors
- [ ] Single-message view: one prominent row `[Alec]> Hello!` clearly visible as primary element
- [ ] Inbox card view: outer black box boundaries clearly visible — you can tell where the list ends
- [ ] Page navigation (scrolling) updates correctly across page headers P 1/3 → P 2/3 etc.
- [ ] MODE tap toggles between single/inbox views with correct layout switch
- [ ] Message sender shown in brackets, text displayed correctly

## What stays unchanged

- `inboxStore()` / `inboxGet()` / `inboxCount()` — RTC RAM inbox data access
- Inbox packet handling for TXT/TXT_MULTI types in `handlePacket()`
- Buddy list/call sign lookup (existing display format updated by this story)

## Story Index Dependencies

- **Required before:** S1 (primitives), S2 (frame engine)
- **Blocks:** none (independent of other mode stories)
