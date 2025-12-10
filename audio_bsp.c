#include <stdio.h>
#include "audio_bsp.h"
#include "freertos/FreeRTOS.h"
#include "driver/i2s_std.h"
#include "user_config.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "driver/i2s_pdm.h"

static const char *TAG = "user_audio";

#define volumeMax 50
static uint8_t volume = 10;

static i2s_chan_handle_t tx_chan; // I2S tx channel handler
static i2s_chan_handle_t rx_chan; // I2S rx channel handler

#define I2S_SAMPLE_RATE 44100
#define I2S_BIT_WIDTH I2S_DATA_BIT_WIDTH_16BIT
#define I2S_CHANNEL_FORMAT I2S_SLOT_MODE_STEREO  // Changed to STEREO for PCM5100A

// ... rest of your defines ...

static void i2s_example_init_std_simplex(void)
{
    // CRITICAL: Unmute the PCM5100A DAC first!
    ESP_LOGI(TAG, "Unmuting PCM5100A DAC (XSMT HIGH)");
    gpio_set_direction(DAC_XSMT_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(DAC_XSMT_PIN, 1);  // HIGH = unmute
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Create I2S TX channel
    i2s_chan_config_t tx_chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
    tx_chan_cfg.auto_clear = true;
    ESP_ERROR_CHECK(i2s_new_channel(&tx_chan_cfg, &tx_chan, NULL));
    
    // Configure I2S for PCM5100A (use Philips I2S format, not MSB)
    i2s_std_config_t tx_std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(I2S_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_BIT_WIDTH, I2S_CHANNEL_FORMAT),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,  // PCM5100A uses internal PLL
            .bclk = EXAMPLE_I2S_STD_BCLK_PIN,
            .ws = EXAMPLE_I2S_STD_WS_PIN,
            .dout = EXAMPLE_I2S_STD_DOUT_PIN,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_chan, &tx_std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(tx_chan));
    ESP_LOGI(TAG, "I2S audio output initialized (GPIO39=BCK, GPIO40=WS, GPIO41=DOUT)");
}

// ... keep rest of your audio_bsp.c code ...
