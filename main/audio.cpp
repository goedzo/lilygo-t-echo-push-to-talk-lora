/* Disabled, because T-Echo has no audio hardware. Replace with bluetooth speakers if possible */
#include "audio.h"


void capAudio(int16_t *buf, uint16_t len) {
/*    size_t bytes_read;
    // Call to NRF52840 specific I2S read function
    nrf_drv_i2s_buffers_t buffers = {
        .p_rx_buffer = buf,
        .p_tx_buffer = NULL
    };
    nrf_drv_i2s_start(&buffers, len, 0);
    nrf_drv_i2s_stop();
    */
}

void playAudio(int16_t *buf, uint16_t len) {
/*
    size_t bytes_written;
    // Call to NRF52840 specific I2S write function
    nrf_drv_i2s_buffers_t buffers = {
        .p_rx_buffer = NULL,
        .p_tx_buffer = buf
    };
    nrf_drv_i2s_start(&buffers, len, 0);
    nrf_drv_i2s_stop();
    */
}
