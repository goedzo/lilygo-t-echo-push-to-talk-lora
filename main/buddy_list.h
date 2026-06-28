#ifndef BUDDY_LIST_H
#define BUDDY_LIST_H

#include <stdint.h>
#include <Arduino.h>

// Buddy list storage in RTC RAM — placed after text_inbox (0x200075C0..0x20007BFF), starts at 0x20007C00
// Each contact: 16-byte call_sign + 8-byte deviceId = 24 bytes + 4 byte header = 28 bytes rounded to 32
#define BUDDY_RAM_BASE              ((volatile uint8_t*)0x20007C00)
#define BUDDY_MAX_CONTACTS          16
#define BUDDY_CALL_SIGN_LEN         16
#define BUDDY_DEVICE_ID_LEN         8
#define CONTACT_SLOT_SIZE           32

typedef struct __attribute__((packed)) {
    uint32_t magic;               // 'BDLY' = 0x42444C59
    uint8_t count;                // number of entries
} BuddyHeader;

typedef struct __attribute__((packed)) {
    char    call_sign[BUDDY_CALL_SIGN_LEN];  // null-terminated, up to 16 chars
    char    device_id[BUDDY_DEVICE_ID_LEN];  // hex short ID (8 chars + null)
} BuddyContact;

// API — all calls are non-blocking, use static buffers
void buddyInit();
void buddyAddOrUpdate(const char* deviceId, const char* callSign);
const char* buddyLookupName(const char* deviceId);   // returns persistent pointer or deviceId itself if unknown
const char* buddyLookupIdByCallSign(const char* sign); // reverse lookup

// Export / import: formats as "CN{call_sign}|DI{device_id},..." for GATT transfer (CSV in one string)
bool buddyExportCsv(char* out_buf, uint16_t max_len);
int  buddyImportCsv(const char* csv_str);               // returns count of parsed contacts

// Get/set local display name (stored as first entry with device_id == our own short ID)
void buddySetDisplayName(const char* name);
const char* buddyGetDisplayName();

#endif
