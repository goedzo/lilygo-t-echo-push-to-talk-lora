#ifndef TEXT_INBOX_H
#define TEXT_INBOX_H

#include <stdint.h>
#include <Arduino.h>

// Inbox configuration — RTC RAM budget: ~1.5KB available below crash_debug area (0x20007FC0)
// Each message: 9-byte envelope + payload. At 512 bytes payload = 521 bytes each.
// 32 messages * 521 ≈ 16.7KB — too much for RTC RAM. Use compact layout.
#define INBOX_MAX_MESSAGES    8   // Compact: 8 messages * ~4KB total (fits RTC RAM)
#define INBOX_MAX_MSG_LEN     256 // Max payload per message

// RTC RAM address range (below crash_debug at 0x20007FC0, above debug buffer at 0x20006FC0)
#define INBOX_RAM_BASE        ((volatile uint8_t*)0x200075C0)

// Layout constants
#define HEADER_SIZE           8   // magic(4) + count(1) + padding(3)
#define SLOT_ENTRY_SIZE       sizeof(InboxSlotEnv)
#define SLOT_COUNT            INBOX_MAX_MESSAGES

// Magic values
#define MAGIC                 0x494E4258   // "INBX"
#define SLOT_MAGIC_LO         0xA           // Low nibble of first byte

// Message state flags
#define MSG_FLAG_INBOX        (1 << 0)     // Currently in inbox (not yet read)
#define MSG_FLAG_TRUNCATED    (1 << 1)     // Payload was truncated

typedef struct __attribute__((packed)) {
    uint8_t   magic_and_src_len;  // bit[3:0]=SLOT_MAGIC_LO, bit[7:4]=src_len
    char        src[4];           // sender short BLE MAC (null-padded)
    uint8_t     msg_len;          // payload length in bytes
    uint8_t     flags;            // MSG_FLAG_* bitmask
} InboxSlotEnv;

typedef struct __attribute__((packed)) {
    uint32_t  magic;
    uint8_t   count;
    uint8_t   _pad[3];
} InboxHeader;

// API
void inboxInit();
bool inboxStore(const char* sender_id, uint8_t sender_len, const uint8_t* payload, uint16_t len);
const char* inboxGet(uint8_t index, uint8_t& out_msg_len, bool& out_truncated, uint8_t (&out_sender)[4]);
uint8_t inboxCount();
bool inboxIsEmpty();
void inboxClear();

// Display helpers for TXT mode
void inboxShowLatest(uint8_t start_line);
void inboxDisplayPage(uint8_t page_start, uint8_t* scroll_cursor, bool needs_refresh);

#endif // TEXT_INBOX_H
