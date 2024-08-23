#ifndef APP_MODES_H
#define APP_MODES_H

enum OperationMode { PTT, TXT, TST, RAW };

void handleAppModes();
void sendAudio();
void sendTestMessage();
void handlePacket();

#endif