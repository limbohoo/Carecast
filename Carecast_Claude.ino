#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <SD_MMC.h>

#include "lvgl.h"
#include "scr_st77916.h"
#include "knob.h"
#include "event_manager.h"
#include "audio_manager.h"

// ====== USER CONFIG ======
#define SD_CS_PIN   4          // adjust to your board
// Wi‑Fi + API placeholders
const char* WIFI_SSID     = "雪松里苑 Cederlaan";
const char* WIFI_PASSWORD = "20252026";
const char* API_URL       = "a4d2d8b85633712f5caa4fc5ef58a37b"; // your endpoint

// 30‑minute ambient schedule
static uint32_t last_ambient_ms    = 0;
static const uint32_t AMBIENT_INTERVAL_MS = 30UL * 60UL * 1000UL;

// periodic online weather refresh (e.g. every 10 min)
static uint32_t last_weather_ms    = 0;
static const uint32_t WEATHER_INTERVAL_MS = 10UL * 60UL * 1000UL;

// ====== WEATHER HELPERS ======

static const char *CITY_EHV = "Eindhoven";
static const char *CITY_TW  = "Taiwan";
static const char *CITY_IT  = "Italy";

// fallback winter preset if Wi‑Fi / API not available
static void apply_default_winter()
{
  update_my_weather(4,  CITY_EHV);  // 4 °C, cloudy
  update_friend_weather(0, 15, CITY_TW);
  update_friend_weather(1, 8,  CITY_IT);
}

// called by your HTTP/JSON code when you have fresh data
static void handle_weather_from_api(float myTemp,
                                    const char *myCity,
                                    float twTemp,
                                    float itTemp)
{
  update_my_weather((int)roundf(myTemp), myCity);
  update_friend_weather(0, (int)roundf(twTemp), CITY_TW);
  update_friend_weather(1, (int)roundf(itTemp), CITY_IT);
}

// fetch online weather, else fallback
static void refresh_weather()
{
  bool ok = false;

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(API_URL);
    int code = http.GET();
    if (code == HTTP_CODE_OK) {
      String payload = http.getString();
      // TODO: parse JSON payload here and fill real values
      // float myT = ..., twT = ..., itT = ...;
      // String myCity = ...;
      // handle_weather_from_api(myT, myCity.c_str(), twT, itT);
      // ok = true;
    }
    http.end();
  }

  if (!ok) {
    apply_default_winter();
  }
}

// ====== SETUP / LOOP ======

void setup()
{
  Serial.begin(115200);
  delay(200);

  // --- Wi‑Fi (optional; safe if it fails) ---
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 8000) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected");
  } else {
    Serial.println("WiFi failed, will use fallback weather");
  }

  // --- SD card initialization for SDMMC (1-bit mode) ---
  SD_MMC.setPins(3, 2, 4);  // CLK=GPIO3(CMD), CMD=GPIO2(D0), D0=GPIO4(D1)
  
  if (!SD_MMC.begin("/sdcard", true)) {  // true = 1-bit mode
    Serial.println("SD_MMC init failed");
  } else {
    Serial.println("SD_MMC OK");
    Serial.printf("SD Size: %lluMB\n", SD_MMC.cardSize() / (1024 * 1024));
  }

  // --- LVGL + display + touch + encoder ---
  scr_lvgl_init();     // from scr_st77916.h

  // --- UI pages (ambient + radar) ---
  knob_init_system();  // from knob.c

  // --- Event and audio managers ---
  event_manager_init();
  audio_manager_init();

  // initial weather data (online or preset)
  refresh_weather();
  last_weather_ms = millis();
  last_ambient_ms = millis();
}

void loop()
{
  // LVGL core
  lv_timer_handler();

  // process one pending high‑level event (audio / notifications)
  event_process_once();

  uint32_t now = millis();

  // 30‑minute ambient forecast scheduling
  if (now - last_ambient_ms > AMBIENT_INTERVAL_MS) {
    last_ambient_ms = now;
    event_post(EVT_TIMER_30MIN, EV_PRIO_LOW, 0, nullptr);
  }

  // periodic weather refresh (if Wi‑Fi available)
  if (now - last_weather_ms > WEATHER_INTERVAL_MS) {
    last_weather_ms = now;
    refresh_weather();
  }

  delay(5);
}
