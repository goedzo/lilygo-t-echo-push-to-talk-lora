#ifndef SCREEN_SYNC_H
#define SCREEN_SYNC_H

// Maximum size of a single sync payload (local stack buffer).
// sendNotificationToApp() handles fragmentation when the wrapped message exceeds MTU.
// 1024 covers every mode including BEACON with max roster + WP broadcast duration.
#define SYNC_MAX_PAYLOAD 1024

void sendScreenSync();
void sendScreenSyncIfDirty();
void sendScreenSyncForced();
void markScreenDirty();
int getPendingNotificationCount();

#endif
