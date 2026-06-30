# S7 — TST Mode Layout (Dashboard Grid)

## Context

TST mode auto-sends "test{n}" every 5s and tracks sent/received counts. Current display shows hardcoded lines with raw counters and GPS data. This story replaces it with a dashboard grid: two white-on-black side-by-side boxes for SENT/RCVD comparison, plus metrics below.

**No changes to `sendTestMessage()`, test counter logic, or packet format (`test{n}`).** Pure display change.

## Layout spec (exact pixel positions)

```
y=12  [tst_icon] ┌─TEST MODE─┐  chn:A spf:7    ← header row (shared)
y=44  ┌──────┐   ┌──────┐                         ← two black boxes side-by-side
y=46      SENT           RCVD                     ← white text centered in each box
y=50     test47          42                        ← counters centered in boxes
y=108  Loss: 5/89 ok                               ← loss line below boxes
y=124  SNR: -9.5dB RSSI: -67dBm                   ← signal metrics
y=140  Last: test47                                ← last received message content
```

### Box layout (each box)

Each box is ~50px wide × 20px tall, white-on-black filled rect with centered text. Two boxes at same Y position side by side with ~10px gap between them.

```
Box dimensions: x=40, y=44, w=52, h=24 (each)
Gap: 8px between boxes
Total span: x=40 to x=160 (120px total width centered on 200px screen)
```

### Rules

- Two boxes are the primary visual element — the comparison IS the point of TST mode
- White text inside black-filled boxes = emphasis (white-on-black per design rules)
- Counters displayed below labels inside same box
- No line separator between boxes and metrics below — just spacing from font height
- "Loss: 5/89 ok" format: total loss / total sent + status word

## Data sources (existing, unchanged)

- `test_message_counter` — auto-incremented counter for sent messages
- `pckt_count` / `rcv_test_message_counter` — received packet count
- `radio->getSNR()` / `radio->getRSSI()` — signal metrics  
- Last received message content (from `handlePacket()` → `updDisp(7, packet.content)`)

## Implementation

In `display_layout.cpp`:

```cpp
void drawTstLayout() {
    // Two side-by-side boxes at y=44
    int box_w = 52, box_h = 24, box_gap = 8;
    int total_box_width = box_w * 2 + box_gap;
    int start_x = (disp_width - total_box_width) / 2;  // centered horizontally
    
    int left_x = start_x;
    int right_x = start_x + box_w + box_gap;
    
    // Left box: SENT
    display->fillRect(left_x, 44, box_w, box_h, GxEPD_BLACK);
    display->drawRect(left_x, 44, box_w, box_h, GxEPD_WHITE);
    
    int label_y = 50;  // approximate center Y in box
    display->setCursor(left_x + (box_w - 28) / 2, label_y - 6);
    display->setTextColor(GxEPD_WHITE);
    display->setFont(&FreeMonoBold9pt7b);
    display->print("SENT");
    
    int counter_y = label_y + 4;  // below label
    char sent_buf[16];
    snprintf(sent_buf, sizeof(sent_buf), "%d", test_message_counter);
    display->setCursor(left_x + (box_w - strlen(sent_buf) * 7) / 2, counter_y + 2);
    display->print(sent_buf);

    // Right box: RCVD
    display->fillRect(right_x, 44, box_w, box_h, GxEPD_BLACK);
    display->drawRect(right_x, 44, box_w, box_h, GxEPD_WHITE);
    
    display->setCursor(right_x + (box_w - 32) / 2, label_y - 6);
    display->print("RCVD");
    
    char rcvd_buf[16];
    snprintf(rcvd_buf, sizeof(rcvd_buf), "%d", rcv_test_message_counter);
    display->setCursor(right_x + (box_w - strlen(rcvd_buf) * 7) / 2, counter_y + 2);
    display->print(rcvd_buf);

    // Reset text color for content below
    display->setTextColor(GxEPD_BLACK);

    // Loss line at y=108
    int total_sent = test_message_counter;
    int loss_count = total_sent > rcv_test_message_counter ? (total_sent - rcv_test_message_counter) : 0;
    char loss_buf[40];
    snprintf(loss_buf, sizeof(loss_buf), "Loss: %d/%d ok", loss_count, total_sent);
    drawSecondaryRow(loss_buf, 108);

    // Signal metrics at y=124
    char sig_buf[50];
    snprintf(sig_buf, sizeof(sig_buf), "SNR: %.1fdB RSSI: %.1fdBm", 
             radio->getSNR(), radio->getRSSI());
    drawSecondaryRow(sig_buf, 124);

    // Last message at y=140
    if (last_test_message_available) {
        char last_buf[30];
        snprintf(last_buf, sizeof(last_buf), "Last: %s", last_test_message_content.c_str());
        drawSecondaryRow(last_buf, 140);
    }

    // Clear remaining lines
}
```

## Testing & Verification

- [ ] `build_scripts\01_build_firmware.bat` compiles with zero errors
- [ ] On device: two clearly visible black boxes side by side — SENT/RCVD comparison is the visual focus
- [ ] Counters update every 5s without full-screen refresh (partial window of box region)
- [ ] Loss count shows correctly (sent - received)
- [ ] Signal metrics and last message display below boxes

## What stays unchanged

- `sendTestMessage()` — auto-sends every 5s
- Test counter increment logic (`test_message_counter++`)
- Packet format: `test{n}`
- Touch-to-reset behavior on TOUCH_PIN

## Story Index Dependencies

- **Required before:** S1 (primitives), S2 (frame engine)
- **Blocks:** none (independent of other mode stories)
