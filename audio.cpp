#include "audio.h"
#include <nrf_drv_i2s.h>

// I2S Configuration for NRF52840
void setupI2S() {
    nrf_drv_i2s_config_t config = NRF_DRV_I2S_DEFAULT_CONFIG;
    config.sck_pin = NRF_GPIO_PIN_MAP(0, 29);  // SCK
    config.lrck_pin = NRF_GPIO_PIN_MAP(0, 28); // LRCK
    config.mck_pin = NRF_GPIO_PIN_MAP(0, 27);  // MCK (optional)
    config.sdout_pin = NRF_GPIO_PIN_MAP(0, 26); // SDOUT
    config.sdin_pin = NRF_GPIO_PIN_MAP(0, 25);  // SDIN

    ret_code_t err_code = nrf_drv_i2s_init(&config, NULL);
    APP_ERROR_CHECK(err_code);
}

void capAudio(int16_t *buf, uint16_t len) {
    size_t bytes_read;
    // Call to NRF52840 specific I2S read function
    nrf_drv_i2s_buffers_t buffers = {
        .p_rx_buffer = buf,
        .p_tx_buffer = NULL
    };
    nrf_drv_i2s_start(&buffers, len, 0);
    nrf_drv_i2s_stop();
}

void playAudio(int16_t *buf, uint16_t len) {
    size_t bytes_written;
    // Call to NRF52840 specific I2S write function
    nrf_drv_i2s_buffers_t buffers = {
        .p_rx_buffer = NULL,
        .p_tx_buffer = buf
    };
    nrf_drv_i2s_start(&buffers, len, 0);
    nrf_drv_i2s_stop();
}