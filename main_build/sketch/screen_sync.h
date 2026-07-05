#line 1 "V:\\Bmad\\Project_ptt_lora\\main\\screen_sync.h"
#ifndef SCREEN_SYNC_H
#define SCREEN_SYNC_H

// Maximum size of a single sync payload (fits in BLE GATT notify, well under MTU)
#define SYNC_MAX_PAYLOAD 200

void sendScreenSync();

#endif
