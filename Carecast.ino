#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SD_MMC.h>

#include "lvgl.h"
#include "scr_st77916.h"
#include "knob.h"
#include "event_manager.h"
#include "audio_manager.h"

// ====== USER CONFIG ======
const char* WIFI_SSID     = "Nothing3a";
const char* WIFI_PASSWORD = "20252026";

// OpenWeatherMap API (free tier)
const char* API_KEY = "a4d2d8b85633712f5caa4fc5ef58a37b";
const char* API_BASE = "http://api.openweathermap.org/data/2.5/weather";

// Locations
const char* CITY_EHV = "Eindhoven,NL";
const char* CITY_TW  = "Taipei,TW";
const char* CITY_IT  = "Genoa,IT";

// 30-minute ambient schedule
static uint32_t last_ambient_ms    = 0;
static const uint32_t AMBIENT_INTERVAL_MS = 30UL * 60UL * 1000UL;

// periodic online weather refresh (every 10 min)
static uint32_t last_weather_ms    = 0;
static const uint32_t WEATHER_INTERVAL_MS = 10UL * 60UL * 1000UL;

// ====== WEATHER HELPERS ======

// fallback winter preset if WiFi / API not available
static void apply_default_winter()
{
  Serial.println("Using fallback weather data");
  update_my_weather(4,  CITY_EHV);  // 4 °C, cloudy
  update_friend_weather(0, 15, "Taiwan");
  update_friend_weather(1, 8,  "Italy");
}

// Fetch weather from OpenWeatherMap API
static bool fetch_weather_for_city(const char* city, float &temp, String &name)
{
  if (WiFi.status() != WL_CONNECTED) return false;

  HTTPClient http;
  String url = String(API_BASE) + "?q=" + city + "&appid=" + API_KEY + "&units=metric";
  
  http.begin(url);
  int code = http.GET();
  
  if (code == HTTP_CODE_OK) {
    String payload = http.getString();
    
    // Parse JSON
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error) {
      temp = doc["main"]["temp"];
      name = doc["name"].as<String>();
      http.end();
      return true;
    } else {
      Serial.printf("JSON parse error for %s: %s\n", city, error.c_str());
    }
  } else {
    Serial.printf("HTTP error for %s: %d\n", city, code);
  }
  
  http.end();
  return false;
}

// fetch online weather, else fallback
static void refresh_weather()
{
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected, using fallback");
    apply_default_winter();
    return;
  }

  float myTemp = 0, twTemp = 0, itTemp = 0;
  String myCity, twCity, itCity;
  
  bool ok1 = fetch_weather_for_city(CITY_EHV, myTemp, myCity);
  bool ok2 = fetch_weather_for_city(CITY_TW, twTemp, twCity);
  bool ok3 = fetch_weather_for_city(CITY_IT, itTemp, itCity);

  if (ok1 && ok2 && ok3) {
    Serial.println("Weather API success");
    update_my_weather((int)roundf(myTemp), myCity.c_str());
    update_friend_weather(0, (int)roundf(twTemp), twCity.c_str());
    update_friend_weather(1, (int)roundf(itTemp), itCity.c_str());
  } else {
    Serial.println("API failed, using fallback");
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
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi connected: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi failed, will use fallback weather");
  }

  // --- SD card for audio files (SDMMC on ESP32-S3) ---
  // Based on your schematic: GPIO2=D0, GPIO3=CMD, GPIO4=CLK (but check your exact pins)
  SD_MMC.setPins(4, 2, 3);  // CLK, D0, CMD
  
  if (!SD_MMC.begin("/sdcard", true, false, SDMMC_FREQ_DEFAULT, 5)) {
    Serial.println("SD_MMC init failed, audio may not work");
  } else {
    Serial.println("SD_MMC OK");
    Serial.printf("SD Size: %lluMB\n", SD_MMC.cardSize() / (1024 * 1024));
  }

  // --- LVGL + display + touch + encoder ---
  scr_lvgl_init();

  // --- UI pages (ambient + radar) ---
  knob_init_system();

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
