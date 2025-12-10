/*
 * 项目名称: Carecast Weather Sonification
 * 硬件: ESP32-S3 (主控) + ESP32-U4WDH (底座) + PCM5100A DAC
 * 功能: 
 * 1. 初始化 SD 卡 (1-bit 模式)
 * 2. 连接 WiFi 获取天气
 * 3. 驱动圆屏 LVGL
 * 4. 播放 WAV 音频 (通过硬件开关 GPIO 0 切换 DAC)
 */

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SD_MMC.h>
#include <math.h>

// 引入本地头文件
#include "lvgl.h"
#include "scr_st77916.h"
#include "knob.h"
#include "event_manager.h"
#include "audio_manager.h" // 关键：引入刚才修改好的音频管理器
#include "user_config.h"

// ====== 用户配置 ======
const char* WIFI_SSID     = "雪松里苑 Cederlaan";
const char* WIFI_PASSWORD = "20252026";

// OpenWeatherMap API
const char* API_KEY = "a4d2d8b85633712f5caa4fc5ef58a37b";
const char* API_BASE = "http://api.openweathermap.org/data/2.5/weather";

// 城市定义
const char* CITY_EHV = "Eindhoven,NL";
const char* CITY_TW  = "Taipei,TW";
const char* CITY_IT  = "Genoa,IT";

// 定时器变量
static uint32_t last_ambient_ms    = 0;
static const uint32_t AMBIENT_INTERVAL_MS = 30UL * 60UL * 1000UL; // 30分钟

static uint32_t last_weather_ms    = 0;
static const uint32_t WEATHER_INTERVAL_MS = 10UL * 60UL * 1000UL; // 10分钟

// 系统状态标志
static bool sd_card_ok = false;
static bool audio_ok = false;
static bool wifi_ok = false;

// ====== SD 卡初始化函数 ======
bool init_sd_card() {
  Serial.println("\n=== SD Card Init ===");
  
  // 硬件配置: CLK=4, CMD=3, D0=5
  SD_MMC.setPins(4, 3, 5);
  
  // 挂载 SD 卡 (使用 "/sdcard" 路径, true 表示 1-bit 模式)
  if (!SD_MMC.begin("/sdcard", true)) {
    Serial.println("❌ SD_MMC 初始化失败!");
    Serial.println("   检查: 卡是否插入? 格式是否为 FAT32? 接触是否良好?");
    return false;
  }
  
  Serial.println("✅ SD_MMC 初始化成功");
  uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
  Serial.printf("   容量: %llu MB\n", cardSize);
  
  return true;
}

// ====== WiFi 初始化函数 ======
bool init_wifi() {
  Serial.println("\n=== WiFi Init ===");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  Serial.print("Connecting");
  uint32_t start = millis();
  
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("✅ WiFi 已连接 IP: %s\n", WiFi.localIP().toString().c_str());
    return true;
  } else {
    Serial.println("⚠️ WiFi 连接超时，将使用离线默认数据");
    return false;
  }
}

// ====== 天气 API 函数 ======

// 默认冬天数据 (离线用)
static void apply_default_winter() {
  Serial.println("📊 应用默认天气数据 (离线)");
  update_my_weather(4,  CITY_EHV);
  update_friend_weather(0, 15, "Taiwan");
  update_friend_weather(1, 8,  "Italy");
}

// 获取单个城市天气
static bool fetch_weather_for_city(const char* city, float &temp, String &name) {
  if (WiFi.status() != WL_CONNECTED) return false;

  HTTPClient http;
  String url = String(API_BASE) + "?q=" + city + "&appid=" + API_KEY + "&units=metric";
  
  http.begin(url);
  http.setTimeout(3000);
  int code = http.GET();
  
  if (code == HTTP_CODE_OK) {
    String payload = http.getString();
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error) {
      temp = doc["main"]["temp"];
      name = doc["name"].as<String>();
      http.end();
      return true;
    }
  }
  http.end();
  return false;
}

// 刷新所有天气
static void refresh_weather() {
  Serial.println("\n🌦️  正在刷新天气...");
  
  if (!wifi_ok || WiFi.status() != WL_CONNECTED) {
    apply_default_winter();
    return;
  }

  float myTemp = 0, twTemp = 0, itTemp = 0;
  String myCity, twCity, itCity;
  
  // 依次获取三个城市
  bool ok1 = fetch_weather_for_city(CITY_EHV, myTemp, myCity);
  bool ok2 = fetch_weather_for_city(CITY_TW, twTemp, twCity);
  bool ok3 = fetch_weather_for_city(CITY_IT, itTemp, itCity);

  if (ok1 && ok2 && ok3) {
    Serial.println("✅ 天气更新成功");
    update_my_weather((int)roundf(myTemp), myCity.c_str());
    update_friend_weather(0, (int)roundf(twTemp), twCity.c_str());
    update_friend_weather(1, (int)roundf(itTemp), itCity.c_str());
  } else {
    Serial.println("⚠️ 部分 API 失败，使用默认值");
    apply_default_winter();
  }
}

// ====== 对外提供的辅助函数 ======

// 这个函数会被 event_manager 调用
void play_weather_sound(const char* soundfile) {
  // 直接调用 audio_manager 里的函数，无需再次检查 sd_card_ok
  // 因为 audio_manager 内部已经知道了状态
  audio_play_file(soundfile);
}

void update_display_status(const char* message) {
  // 这里可以添加更新 UI 状态栏的代码
  Serial.printf("Status: %s\n", message);
}

// ====== 主程序 SETUP ======

void setup() {
  Serial.begin(115200);
  delay(1000); // 等待串口稳定

  Serial.println("\n╔═══════════════════════════════════╗");
  Serial.println("║   CARECAST SYSTEM STARTING...     ║");
  Serial.println("╚═══════════════════════════════════╝");

  // 1. 初始化 SD 卡
  sd_card_ok = init_sd_card();
  
  // 2. 【关键】把 SD 卡状态告诉音频管理器
  // 这解决了 "sd_card_ok not declared" 的问题
  audio_set_sd_status(sd_card_ok);

  // 3. 初始化音频管理器 (I2S, GPIO 0 开关)
  // 这会接管 DAC 控制权
  if (audio_manager_init()) {
    audio_ok = true;
    Serial.println("✅ 音频系统初始化完成");
  } else {
    Serial.println("❌ 音频系统初始化失败");
  }

  // 4. 初始化 WiFi
  wifi_ok = init_wifi();

  // 5. 初始化显示屏 (LVGL)
  Serial.println("=== Display Init ===");
  scr_lvgl_init();

  // 6. 初始化 UI 页面
  knob_init_system();

  // 7. 初始化事件管理器
  event_manager_init();

  // 8. 首次加载数据
  refresh_weather();
  last_weather_ms = millis();
  last_ambient_ms = millis();

  // 系统就绪提示音 (如果有 startup.wav)
  if (sd_card_ok && audio_ok) {
     // audio_play_file("/startup.wav"); 
  }

  Serial.println("\n🚀 系统启动完成! 输入 't' 测试音频, 'w' 刷新天气");
}

// ====== 主程序 LOOP ======

void loop() {
  // 1. LVGL 心跳 (处理屏幕显示)
  lv_timer_handler();

  // 2. 处理事件队列 (播放音频、切换界面等)
  event_process_once();

  uint32_t now = millis();

  // 3. 定时任务：30分钟环境音
  if (now - last_ambient_ms > AMBIENT_INTERVAL_MS) {
    last_ambient_ms = now;
    Serial.println("⏰ 触发环境音效...");
    event_post(EVT_TIMER_30MIN, EV_PRIO_LOW, 0, nullptr);
  }

  // 4. 定时任务：10分钟刷新天气
  if (now - last_weather_ms > WEATHER_INTERVAL_MS) {
    last_weather_ms = now;
    if (wifi_ok && WiFi.status() == WL_CONNECTED) {
      refresh_weather();
    }
  }

  // 5. 串口调试指令
  if (Serial.available()) {
    char cmd = Serial.read();
    if (cmd == 't') {
        Serial.println("🔊测试播放: /audio/eindhoven_rainyday.wav");
        audio_play_file("/audio/eindhoven_rainyday.wav");
    } else if (cmd == 'w') {
        refresh_weather();
    } else if (cmd == 'f') {
        // 列出文件
        if (sd_card_ok) {
            File root = SD_MMC.open("/");
            File file = root.openNextFile();
            while(file){
                Serial.printf("FILE: %s (%d)\n", file.name(), file.size());
                file = root.openNextFile();
            }
        }
    }
  }

  delay(5); // 给 CPU 一点喘息时间
}