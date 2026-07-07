#ifndef SCREEN_SYNC_H
#define SCREEN_SYNC_H

// Maximum size of a single sync payload (BLE GATT characteristic max is 253 bytes)
// Payload must fit in: PREFIX(16) + SYNC_MAX_PAYLOAD + ~~(2) + margin <= 247 (negotiated MTU)
// So max payload = 247 - 16 - 2 - 4 = 225, use 230 for safety with clamping in drainQueue()
#define SYNC_MAX_PAYLOAD 230

void sendScreenSync();
void sendScreenSyncIfDirty();
void markScreenDirty();
int getPendingNotificationCount();

#endif
