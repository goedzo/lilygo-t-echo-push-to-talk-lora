#ifndef APP_MODES_H
#define APP_MODES_H

enum OperationMode { PTT, TXT, TST, RAW };

extern OperationMode current_mode;
extern char channels[];
extern int channel_idx;
extern int volume_level;

void handleAppModes();
void updMode();
void updChannel();

#endif