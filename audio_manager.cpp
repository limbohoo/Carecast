#include "audio_manager.h"
#include "event_manager.h"
#include "pincfg.h"

// include your chosen I2S audio library here
// #include <Audio.h>

static audio_state_t g_state = AUDIO_IDLE;
static uint8_t g_volume = 18;   // 0..21 for ESP32-audioI2S

// simple helpers to map logical sounds to files
static const char *FILE_AMBIENT   = "/ambient/next_hour.wav";
static const char *FILE_TOUCH_NOW = "/ambient/current_now.wav";

static const char *friend_file_for(int friend_id)
{
    // e.g. /friends/01_forecast.wav etc.
    // keep it simple and deterministic
    switch (friend_id) {
    case 0: return "/friends/00_tina.wav";
    case 1: return "/friends/01_lucia.wav";
    default: return "/friends/99_default.wav";
    }
}

static const char *NOTIFY_PING_FILE = "/system/notify_ping.wav";
static const char *SEND_OK_FILE     = "/system/send_ok.wav";

// Audio audio;  // if using ESP32-audioI2S

static void play_file(const char *path, audio_state_t new_state)
{
    // TODO: implement cross‑fade if you want;
    // for now, just stop & start
    // audio.stopSong();
    // audio.connecttoFS(SD, path);

    g_state = new_state;
}

/* public API */

void audio_manager_init(void)
{
    // audio.setPinout(AUDIO_I2S_BCK_IO, AUDIO_I2S_WS_IO, AUDIO_I2S_DO_IO);
    // audio.setVolume(g_volume);
    g_state = AUDIO_IDLE;
}

bool audio_can_start_ambient(void)
{
    // ambient may run only when nothing more important is playing
    return g_state == AUDIO_IDLE || g_state == AUDIO_AMBIENT_FORECAST;
}

void audio_play_ambient_forecast(void)
{
    if (!audio_can_start_ambient()) return;
    play_file(FILE_AMBIENT, AUDIO_AMBIENT_FORECAST);
}

void audio_play_manual_current(void)
{
    // touch should interrupt everything
    play_file(FILE_TOUCH_NOW, AUDIO_MANUAL_CURRENT);
}

void audio_play_friend_weather(int friend_id)
{
    const char *file = friend_file_for(friend_id);
    play_file(file, AUDIO_FRIEND_WEATHER);
}

void audio_play_friend_send_confirm(void)
{
    play_file(SEND_OK_FILE, AUDIO_NOTIFICATION_PING);
}

void audio_handle_friend_notification(int friend_id)
{
    // visual notification is handled elsewhere
    // here: optionally soft ping if idle
    if (g_state == AUDIO_IDLE || g_state == AUDIO_AMBIENT_FORECAST) {
        play_file(NOTIFY_PING_FILE, AUDIO_NOTIFICATION_PING);
    }
}

void audio_on_finished(void)
{
    // called when the underlying player signals end‑of‑file
    g_state = AUDIO_IDLE;
}
