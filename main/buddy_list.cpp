#include "buddy_list.h"
#include <cstring>
#include <cstdint>

static volatile uint8_t* buddy_base = nullptr;
static bool g_buddy_inited = false;

#define MAGIC_BDLY 0x42444C59  // 'BDLY'

inline static BuddyHeader* _bhdr() {
    return (BuddyHeader*)buddy_base;
}

inline static BuddyContact* _contactPtr(uint8_t idx) {
    uint32_t off = sizeof(BuddyHeader);
    off += (idx * CONTACT_SLOT_SIZE);
    return (BuddyContact*)(buddy_base + off);
}

void buddyInit() {
    buddy_base = (volatile uint8_t*)BUDDY_RAM_BASE;
    
    if (_bhdr()->magic != MAGIC_BDLY) {
        memset((void*)buddy_base, 0, sizeof(BuddyHeader) + (BUDDY_MAX_CONTACTS * CONTACT_SLOT_SIZE));
        _bhdr()->magic = MAGIC_BDLY;
        _bhdr()->count = 0;
    }
    g_buddy_inited = true;
}

void buddyAddOrUpdate(const char* deviceId, const char* callSign) {
    if (!g_buddy_inited) buddyInit();
    if (!buddy_base || !deviceId || !callSign) return;
    
    int len = strlen(callSign);
    if (len == 0 || len > BUDDY_CALL_SIGN_LEN - 1) return;
    uint8_t idLen = strlen(deviceId);
    if (idLen == 0 || idLen > BUDDY_DEVICE_ID_LEN - 1) return;
    
    // Check for existing entry — update call_sign in place
    for (int i = 0; i < _bhdr()->count; i++) {
        BuddyContact* e = _contactPtr(i);
        if (strcmp(e->device_id, deviceId) == 0) {
            strncpy(e->call_sign, callSign, BUDDY_CALL_SIGN_LEN - 1);
            e->call_sign[BUDDY_CALL_SIGN_LEN - 1] = '\0';
            return;
        }
    }
    
    // Add new entry — if full, drop oldest (index 0)
    if (_bhdr()->count >= BUDDY_MAX_CONTACTS) {
        _bhdr()->count--;
        for (int i = 0; i < _bhdr()->count; i++) {
            BuddyContact* dst = _contactPtr(i);
            BuddyContact* src = _contactPtr(i + 1);
            memcpy(dst, src, sizeof(BuddyContact));
        }
    }
    
    int idx = _bhdr()->count++;
    BuddyContact* e = _contactPtr(idx);
    memset(e, 0, sizeof(BuddyContact));
    strncpy(e->call_sign, callSign, BUDDY_CALL_SIGN_LEN - 1);
    strncpy(e->device_id, deviceId, BUDDY_DEVICE_ID_LEN - 1);
}

const char* buddyLookupName(const char* deviceId) {
    if (!g_buddy_inited) buddyInit();
    if (!buddy_base || !deviceId) return "";
    
    for (int i = 0; i < _bhdr()->count; i++) {
        BuddyContact* e = _contactPtr(i);
        if (strcmp(e->device_id, deviceId) == 0 && e->call_sign[0] != '\0') {
            return e->call_sign;
        }
    }
    return "";
}

const char* buddyLookupIdByCallSign(const char* sign) {
    if (!g_buddy_inited) buddyInit();
    if (!buddy_base || !sign) return "";
    
    for (int i = 0; i < _bhdr()->count; i++) {
        BuddyContact* e = _contactPtr(i);
        if (strcmp(e->call_sign, sign) == 0 && e->device_id[0] != '\0') {
            return e->device_id;
        }
    }
    return "";
}

bool buddyExportCsv(char* out_buf, uint16_t max_len) {
    if (!g_buddy_inited) buddyInit();
    if (!buddy_base || !out_buf) return false;
    
    out_buf[0] = '\0';
    uint16_t off = 0;
    
    for (int i = 0; i < _bhdr()->count; i++) {
        BuddyContact* e = _contactPtr(i);
        if (e->call_sign[0] == '\0') continue;
        
        // Format: CN{call_sign}|DI{device_id},...
        int written = snprintf(out_buf + off, max_len - off, "CN%s|DI%s,", 
                               e->call_sign, e->device_id);
        if (written < 0 || (uint16_t)written >= max_len - off) {
            return false;  // Buffer too small
        }
        off += written;
    }
    
    return _bhdr()->count > 0;
}

int buddyImportCsv(const char* csv_str) {
    if (!csv_str || !g_buddy_inited) buddyInit();
    int count = 0;
    
    // Parse CSV: "CNname1|DIid1,CNname2|DIid2,..."
    const char* p = csv_str;
    while (*p && strlen(p) >= 6) {  // minimal entry: CNx|DIxxxxx,
        if (p[0] == 'C' && p[1] == 'N') {
            const char* pipe = strchr(p, '|');
            if (pipe && *(pipe + 1) == 'D') {
                char callSign[BUDDY_CALL_SIGN_LEN];
                char deviceId[BUDDY_DEVICE_ID_LEN];
                uint8_t csLen = (uint8_t)(pipe - p - 2);
                if (csLen > 0 && csLen < BUDDY_CALL_SIGN_LEN) {
                    strncpy(callSign, p + 2, csLen);
                    callSign[csLen] = '\0';
                    
                    const char* comma = strchr(pipe + 1, ',');
                    uint8_t diLen;
                    if (comma) {
                        diLen = (uint8_t)(comma - (pipe + 2));
                    } else {
                        diLen = (uint8_t)strlen(pipe + 2);
                    }
                    
                    if (diLen > 0 && diLen < BUDDY_DEVICE_ID_LEN) {
                        strncpy(deviceId, pipe + 2, diLen);
                        deviceId[diLen] = '\0';
                        
                        buddyAddOrUpdate(deviceId, callSign);
                        count++;
                    }
                }
            }
        }
        
        // Advance past current entry
        const char* comma = strchr(p, ',');
        if (comma) {
            p = comma + 1;
        } else {
            break;
        }
    }
    
    return count;
}

void buddySetDisplayName(const char* name) {
    buddyAddOrUpdate("SELF", name);
}

const char* buddyGetDisplayName() {
    if (!g_buddy_inited) buddyInit();
    // Look for SELF entry — returns static buffer for display
    if (_bhdr()->count > 0) {
        BuddyContact* first = _contactPtr(0);
        if (first && strcmp(first->device_id, "SELF") == 0 && first->call_sign[0] != '\0') {
            return first->call_sign;
        }
    }
    return "";
}
