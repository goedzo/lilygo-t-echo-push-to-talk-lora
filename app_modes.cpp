#include "app_modes.h"
#include "display.h"
#include "lora.h"
#include "audio.h"
#include "settings.h"

OperationMode current_mode = PTT;

void handleAppModes() {
    if (current_mode == PTT) {
        sendAudio();
    } else if (current_mode == TST) {
        sendTestMessage();
    } else if (current_mode == TXT || current_mode == RAW) {
        handlePacket();
    }
}

void sendAudio() {
    while (digitalRead(ACTION_PIN) == LOW) {
        capAudio(raw_buf, RAW_SIZE);
        uint16_t enc_size = codec2_encode(codec, pkt_buf + 4, raw_buf);  

        sendPacket(pkt_buf, enc_size + 4);
        updDisp(1, "Transmitting...");

        handlePacket();
    }
}

void sendTestMessage() {
    char test_msg[20];
    snprintf(test_msg, sizeof(test_msg), "test%d", ++test_message_counter);

    snprintf((char*)pkt_buf, 4, "TX%c", channels[channel_idx]);
    strcpy((char*)pkt_buf + 3, test_msg);
    sendPacket(pkt_buf, strlen(test_msg) + 3);

    char display_msg[30];
    snprintf(display_msg, sizeof(display_msg), "Sent: %s", test_msg);
    updDisp(1, display_msg);
}

void handlePacket() {
    int pkt_size = receivePacket(pkt_buf, MAX_PKT);
    if (pkt_size) {
        pkt_buf[pkt_size] = '\0';

        char expected_ptt_header[4];
        snprintf(expected_ptt_header, sizeof(expected_ptt_header), "PT%c", channels[channel_idx]);

        char expected_txt_header[4];
        snprintf(expected_txt_header, sizeof(expected_txt_header), "TX%c", channels[channel_idx]);

        if (current_mode == RAW) {
            strncpy(message_lines[current_message_line], (char*)pkt_buf, sizeof(message_lines[current_message_line]) - 1);
            current_message_line = (current_message_line + 1) % 10;
            updateMessageDisplay();
        } else if (current_mode == PTT && strncmp((char*)pkt_buf, expected_ptt_header, 3) == 0) {
            uint8_t rcv_mode = pkt_buf[3];
            if (rcv_mode < sizeof(modes) / sizeof(modes[0])) {
                codec = codec2_create(modes[rcv_mode]);
                codec2_decode(codec, raw_buf, pkt_buf + 4);
                playAudio(raw_buf, RAW_SIZE);
                updDisp(1, "Receiving...");
            } else {
                updDisp(2, "Invalid mode received");
            }
        } else if (current_mode == TXT && strncmp((char*)pkt_buf, expected_txt_header, 3) == 0) {
            strncpy(message_lines[current_message_line], (char*)pkt_buf + 3, sizeof(message_lines[current_message_line]) - 1);
            current_message_line = (current_message_line + 1) % 10;
            updateMessageDisplay();
        }
    }
}