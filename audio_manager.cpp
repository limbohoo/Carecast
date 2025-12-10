// audio_manager.cpp
#include "audio_manager.h"
#include "event_manager.h"
#include <driver/i2s_std.h>
#include <driver/gpio.h>
#include <SD_MMC.h>
#include <FS.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

// === 硬件配置 ===
#define I2S_BCK_PIN      GPIO_NUM_39
#define I2S_WS_PIN       GPIO_NUM_40
#define I2S_DATA_PIN     GPIO_NUM_41
#define I2S_SWITCH_PIN   GPIO_NUM_0   // 硬件开关
#define DAC_XSMT_PIN     GPIO_NUM_38  // 静音控制
#define SAMPLE_RATE      44100

static i2s_chan_handle_t tx_chan = NULL;
static bool _sd_ready = false;
static volatile bool _is_playing = false;

// FreeRTOS 任务句柄和队列
TaskHandle_t audioTaskHandle = NULL;
QueueHandle_t audioQueue = NULL;

// 音频播放任务 (运行在后台)
void audioTask(void *parameter) {
    char filePath[64];
    uint8_t buffer[1024];
    size_t bytes_read, bytes_written;

    while (true) {
        // 1. 等待队列中有新的播放请求 (portMAX_DELAY 表示无限等待，不耗 CPU)
        if (xQueueReceive(audioQueue, &filePath, portMAX_DELAY) == pdTRUE) {
            
            if (!_sd_ready) continue;

            File file = SD_MMC.open(filePath);
            if (!file) {
                Serial.printf("[AudioTask] ❌ File not found: %s\n", filePath);
                continue;
            }

            Serial.printf("[AudioTask] 🎵 Start: %s\n", filePath);
            _is_playing = true;

            // 跳过 WAV 头部 (44 bytes)
            file.seek(44);

            // 播放循环
            while (file.available()) {
                // 如果队列里来了新歌，或者接收到停止指令，可以在这里处理 (目前暂不实现打断)
                
                bytes_read = file.read(buffer, sizeof(buffer));
                
                // 写入 I2S (这里会阻塞，直到 DMA 缓冲区有空位，但这只阻塞当前任务，不影响 UI)
                i2s_channel_write(tx_chan, buffer, bytes_read, &bytes_written, portMAX_DELAY);
            }

            // 播放结束清理
            file.close();
            _is_playing = false;
            
            // 稍微等待一下，防止爆音
            vTaskDelay(pdMS_TO_TICKS(50));
            
            Serial.println("[AudioTask] Finished");
            
            // 通知事件管理器 (需要在 event_manager 中处理线程安全，或者直接在这里简单打印)
            // event_notify_audio_complete(); 
        }
    }
}

bool audio_manager_init() {
    Serial.println("[Audio] Init with FreeRTOS Task...");

    // 1. GPIO 配置
    gpio_set_direction(I2S_SWITCH_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(I2S_SWITCH_PIN, 1); // 切换到 S3
    
    gpio_set_direction(DAC_XSMT_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(DAC_XSMT_PIN, 1); // 解除静音

    // 2. I2S 驱动安装 (保持不变)
    i2s_chan_config_t tx_chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    tx_chan_cfg.auto_clear = true;
    if (i2s_new_channel(&tx_chan_cfg, &tx_chan, NULL) != ESP_OK) return false;

    i2s_std_config_t tx_std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_BCK_PIN,
            .ws = I2S_WS_PIN,
            .dout = I2S_DATA_PIN,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {0},
        },
    };
    if (i2s_channel_init_std_mode(tx_chan, &tx_std_cfg) != ESP_OK) return false;
    if (i2s_channel_enable(tx_chan) != ESP_OK) return false;

    // 3. 创建播放队列 (深度为 3，存文件名)
    audioQueue = xQueueCreate(3, 64 * sizeof(char));

    // 4. 创建后台音频任务
    // Stack: 4096, Prio: 2 (比主循环稍高), Handle: &audioTaskHandle, Core: 0 (独立核心)
    xTaskCreatePinnedToCore(
        audioTask, "AudioTask", 4096, NULL, 2, &audioTaskHandle, 0
    );

    return true;
}

void audio_set_sd_status(bool status) { _sd_ready = status; }
bool audio_is_playing() { return _is_playing; }
void audio_stop() { _is_playing = false; /* TODO: Add stop logic to task */ }

// 非阻塞播放接口
void audio_play_file(const char* filePath) {
    if (audioQueue == NULL) return;

    String path = filePath;
    if (!path.startsWith("/")) path = "/" + path;

    // 将文件名发送到队列，立刻返回，不阻塞 UI
    // 如果队列满了（正在播），这里选择不等待 (0)，直接丢弃或者等待
    char pathBuf[64];
    strncpy(pathBuf, path.c_str(), 63);
    pathBuf[63] = '\0';

    if (xQueueSend(audioQueue, &pathBuf, 0) != pdTRUE) {
        Serial.println("[Audio] Queue full, skipping request");
    }
}

// === 业务逻辑 ===
void audio_play_manual_current() { audio_play_file("/audio/eindhoven_rainyday.wav"); }
void audio_play_friend_send_confirm() { audio_play_file("/audio/mist.wav"); }
void audio_play_friend_weather(int friendId) { audio_play_file("/audio/sunny.wav"); }
bool audio_can_start_ambient() { return !_is_playing; }
void audio_play_ambient_forecast() { audio_play_file("/audio/clouds with light rain.wav"); }
void audio_handle_friend_notification(int friendId) { audio_play_file("/audio/thehague_windyafternoon.wav"); }
void audio_on_finished() { }