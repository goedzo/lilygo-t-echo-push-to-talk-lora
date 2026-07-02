#include "text_inbox.h"
#include <cstring>
#include <cstdint>

static volatile uint8_t* inbox_base = nullptr;
static bool g_inited = false;

// Return pointer to slot envelope at index
inline static InboxSlotEnv* _envPtr(uint8_t idx) {
    uint32_t off = HEADER_SIZE + (idx * SLOT_ENTRY_SIZE);
    return (InboxSlotEnv*)(inbox_base + off);
}

// Return pointer to payload at index
inline static volatile uint8_t* _payloadPtr(uint8_t idx) {
    uint32_t env_base = HEADER_SIZE + (SLOT_COUNT * SLOT_ENTRY_SIZE);
    uint32_t off = env_base + (idx * INBOX_MAX_MSG_LEN);
    return inbox_base + off;
}

inline static InboxHeader* _hdr() {
    return (InboxHeader*)inbox_base;
}

void inboxInit() {
    inbox_base = (volatile uint8_t*)INBOX_RAM_BASE;
    
    if (_hdr()->magic != MAGIC) {
        memset((void*)inbox_base, 0, HEADER_SIZE + (SLOT_COUNT * SLOT_ENTRY_SIZE) + (SLOT_COUNT * INBOX_MAX_MSG_LEN));
        _hdr()->magic = MAGIC;
        _hdr()->count = 0;
    }
    g_inited = true;
}

bool inboxStore(const char* sender_id, uint8_t sender_len, const uint8_t* payload, uint16_t len) {
    if (!g_inited) inboxInit();
    if (!inbox_base) return false;
    
    // Enforce max payload
    if (len > INBOX_MAX_MSG_LEN - 1) {
        len = INBOX_MAX_MSG_LEN - 1;
    }
    
    // Compact: if full, drop oldest and shift
    if (_hdr()->count >= SLOT_COUNT) {
        for (int i = 1; i < _hdr()->count; i++) {
            InboxSlotEnv* dst = _envPtr(i - 1);
            InboxSlotEnv* src = _envPtr(i);
            memcpy(dst, src, sizeof(InboxSlotEnv));
            
            volatile uint8_t* dp = _payloadPtr(i - 1);
            volatile uint8_t* sp = _payloadPtr(i);
            memcpy((void*)dp, (const void*)sp, INBOX_MAX_MSG_LEN);
        }
        _hdr()->count--;
    }
    
    int idx = _hdr()->count;
    InboxSlotEnv* env = _envPtr(idx);
    memset((void*)env, 0, sizeof(InboxSlotEnv));
    
    // Set magic (low nibble) and src_len (high nibble) in first byte
    uint8_t s_len = sender_len < 4 ? sender_len : 4;
    env->magic_and_src_len = ((s_len & 0x0F) << 4) | SLOT_MAGIC_LO;
    
    if (sender_id && s_len > 0) {
        memcpy(env->src, sender_id, s_len);
    }
    env->msg_len = (uint8_t)len;
    env->flags = MSG_FLAG_INBOX;
    
    // Store payload
    uint8_t* pp = (uint8_t*)_payloadPtr(idx);
    memcpy(pp, payload, len);
    pp[len] = '\0';
    
    _hdr()->count++;
    return true;
}

const char* inboxGet(uint8_t index, uint8_t& out_msg_len, bool& out_truncated, uint8_t (&out_sender)[4]) {
    if (!g_inited) inboxInit();
    if (index >= _hdr()->count || !inbox_base) return nullptr;
    
    memset(out_sender, 0, 4);
    out_msg_len = 0;
    out_truncated = false;
    
    InboxSlotEnv* env = _envPtr(index);
    // Check magic: first byte low nibble should be 0xA
    if ((env->magic_and_src_len & 0x0F) != SLOT_MAGIC_LO) return nullptr;
    
    uint8_t src_len = (env->magic_and_src_len >> 4) & 0x0F;
    for (int i = 0; i < src_len && i < 4; i++) {
        out_sender[i] = (uint8_t)env->src[i];
    }
    out_msg_len = env->msg_len;
    out_truncated = (env->flags & MSG_FLAG_TRUNCATED) != 0;
    
    return (const char*)_payloadPtr(index);
}

uint8_t inboxCount() {
    if (!g_inited) return 0;
    return _hdr()->count;
}

bool inboxIsEmpty() {
    return inboxCount() == 0;
}

void inboxClear() {
    if (!g_inited) return;
    memset((void*)inbox_base, 0, HEADER_SIZE + (SLOT_COUNT * SLOT_ENTRY_SIZE) + (SLOT_COUNT * INBOX_MAX_MSG_LEN));
    _hdr()->magic = MAGIC;
    _hdr()->count = 0;
}

// Display the most recent message at a given display line
void inboxShowLatest(uint8_t start_line) {
    if (_hdr()->count == 0 || !inbox_base) return;
    
    uint8_t ml = 0, sender[4] = {0};
    bool truncated = false;
    const char* payload = inboxGet(_hdr()->count - 1, ml, truncated, sender);
    
    if (!payload || ml == 0) return;
    
    char display_msg[81];
    int di = 0;
    uint8_t sent_len = (sender[3] >> 4) & 0x0F;
    if (sent_len == 0) sent_len = 4;
    
    display_msg[di++] = '[';
    for (int i = 0; i < sent_len && di < 80; i++) {
        display_msg[di++] = sender[i] & 0xFF;
    }
    if (di < 80) display_msg[di++] = ']';
    if (di < 80) display_msg[di++] = ' ';
    
    for (int i = 0; i < ml && di < 79; i++) {
        char c = payload[i];
        if (isprint((unsigned char)c)) {
            display_msg[di++] = c;
        } else {
            display_msg[di++] = '.';
        }
    }
    if (truncated && di < 79) display_msg[di++] = '~';
    display_msg[di] = '\0';
    
    extern void updDisp(uint8_t line, const char* msg, bool updateScreen);
    updDisp(start_line, display_msg, true);
}

// Display a page of inbox messages on E-Paper (up to 16 lines for message body)
void inboxDisplayPage(uint8_t page_start, uint8_t* scroll_cursor, bool needs_refresh) {
    if (!_hdr() || !inbox_base) return;
    
    int count = _hdr()->count;
    int lines_per_page = 16;  // rows 2-17 on E-Paper
    
    extern void updDisp(uint8_t line, const char* msg, bool updateScreen);
    
    for (int i = 0; i < lines_per_page; i++) {
        int disp_line = 2 + i;
        int msg_idx = page_start + i;
        
        if (msg_idx < count) {
            uint8_t ml = 0, sender[4] = {0};
            bool truncated = false;
            const char* payload = inboxGet(msg_idx, ml, truncated, sender);
            
            if (payload && ml > 0) {
                int di = 0;
                char display_msg[81];
                uint8_t src_len = (sender[3] >> 4) & 0x0F;
                if (src_len == 0) src_len = 4;
                
                display_msg[di++] = '[';
                for (int s = 0; s < src_len && di < 79; s++) {
                    display_msg[di++] = sender[s] & 0xFF;
                }
                if (di < 80) display_msg[di++] = ']';
                if (di < 80) display_msg[di++] = '>';
                
                int remaining = 76 - di;
                for (int p = 0; p < ml && p < remaining && di < 79; p++) {
                    char c = payload[p];
                    display_msg[di++] = isprint((unsigned char)c) ? c : '.';
                }
                display_msg[di] = '\0';
                
                updDisp(disp_line, display_msg, needs_refresh);
            } else {
                updDisp(disp_line, "", false);
            }
        } else {
            updDisp(disp_line, "", false);
        }
    }
    
    if (scroll_cursor) {
        *scroll_cursor = count > (uint8_t)lines_per_page ? page_start : 0;
    }
}

// Helper for layout module — gets message payload into caller buffer
uint8_t inboxGetMessage(uint8_t index, char* out_buf, size_t buf_len) {
    if (!g_inited || !inbox_base) return 0;
    
    uint8_t ml = 0;
    bool truncated = false;
    uint8_t sender[4] = {0};
    const char* payload = inboxGet(index, ml, truncated, sender);
    if (!payload) return 0;
    
    size_t copy_len = (ml < buf_len - 1) ? ml : buf_len - 1;
    memcpy(out_buf, payload, copy_len);
    out_buf[copy_len] = '\0';
    return ml;
}
