#ifndef LORA_H
#define LORA_H

bool setupLoRa();
void sendPacket(uint8_t* pkt_buf, uint16_t len);
int receivePacket(uint8_t* pkt_buf, uint16_t max_len);
void sleepLoRa();

#endif