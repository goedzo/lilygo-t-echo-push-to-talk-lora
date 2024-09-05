#include <stdint.h>
#ifndef AUDIO_H
#define AUDIO_H

void capAudio(int16_t *buf, uint16_t len);
void playAudio(int16_t *buf, uint16_t len);

#endif