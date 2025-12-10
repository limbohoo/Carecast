#ifndef USER_CONFIG_H
#define USER_CONFIG_H

#include <driver/gpio.h>

// ====== I2S AUDIO OUTPUT (ESP32-S3 → U4WDH → PCM5100A) ======
#define EXAMPLE_I2S_STD_BCLK_PIN  (gpio_num_t)39  // BCK
#define EXAMPLE_I2S_STD_WS_PIN    (gpio_num_t)40  // WS/LRCK
#define EXAMPLE_I2S_STD_DOUT_PIN  (gpio_num_t)41  // DIN

// DAC mute control (HIGH = unmuted)
#define DAC_XSMT_PIN              (gpio_num_t)38  // XSMT

// ====== I2S PDM MICROPHONE ======
#define EXAMPLE_I2S_PDM_DATA_PIN  (gpio_num_t)46
#define EXAMPLE_I2S_PDM_CLK_PIN   (gpio_num_t)45

// ====== ROTARY ENCODER ======
#define EXAMPLE_ENCODER_ECA_PIN   8
#define EXAMPLE_ENCODER_ECB_PIN   7

// ====== BIT MANIPULATION ======
#define SET_BIT(reg, bit)   (reg |= ((uint32_t)0x01 << bit))
#define CLEAR_BIT(reg, bit) (reg &= ~((uint32_t)0x01 << bit))
#define READ_BIT(reg, bit)  ((reg >> bit) & 0x01)
#define BIT_EVEN_ALL        (0x00ffffff)

#endif
