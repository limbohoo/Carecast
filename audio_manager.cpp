#include "audio_manager.h"
#include "pincfg.h"
#include <SD_MMC.h>

// TODO: Uncomment when you add your audio library
// #include "Audio.h"

// --- Audio Hardware Config ---
// Update these pins based on your I2S audio output circuit
#define AUDIO_I2S_BCK_IO   12  // Bit clock
#define AUDIO_I2S_WS_IO    13  // Word select (LRCK)
#define AUDIO_I2S_DO_IO    14  // Data out

// --- State ---
static audio_state_t g_state  = AUDIO_IDLE;
static uint8_t       g_volume = 18;  // 0-21 typical range

// --- Audio Library Instance ---
// Audio audio;  // Uncomment when library is added

// --- SD Card File Paths ---
// Ambient soundscapes
static const char *FILE_AMBIENT       = "/ambient/next_hour.wav";
static const char *FILE_TOUCH_NOW     = "/ambient/current_now.wav";

// Friend-specific weather
static const char *FILE_FRIEND_TINA   = "/friends/tina_weather.wav";
static const char *FILE_FRIEND_LUCIA  = "/friends/lucia_weather.wav";

// System sounds
static const char *NOTIFY_PING_FILE   = "/system/notify_ping.wav";
static const char *SEND_OK_FILE       = "/system/send_confirm.wav";

// --- Helper: Select Friend Audio File ---
static const char *friend_file_for(int friend_id)
{
    switch (friend_id) {
    case 0: return FILE_FRIEND_TINA;
    case 1: return FILE_FRIEND_LUCIA;
    default: return FILE_FRIEND_TINA;  // fallback
    }
}

// --- Core Playback Function ---
static void play_file(const char *path, audio_state_t new_state)
{
    // Check if file exists
    if (!SD_MMC.exists(path)) {
        Serial.printf("Audio file not found: %s\n", path);
        g_state = AUDIO_IDLE;
        return;
    }

    Serial.printf("Playing: %s\n", path);

    // TODO: Uncomment when audio library is integrated
    // audio.stopSong();
    // if (audio.connecttoFS(SD_MMC, path)) {
    //     g_state = new_state;
    // } else {
    //     Serial.println("Audio playback failed");
    //     g_state = AUDIO_IDLE;
    // }

    // Temporary: just set state for testing
    g_state = new_state;
}

// --- Initialization ---
void audio_manager_init(void)
{
    Serial.println("Audio Manager Init");

    // TODO: Uncomment when audio library is integrated
    // audio.setPinout(AUDIO_I2S_BCK_IO, AUDIO_I2S_WS_IO, AUDIO_I2S_DO_IO);
    // audio.setVolume(g_volume);  // 0-21

    g_state = AUDIO_IDLE;
    
    // Test if SD_MMC has audio files
    if (SD_MMC.exists("/ambient")) {
        Serial.println("Found /ambient folder");
    }
    if (SD_MMC.exists("/friends")) {
        Serial.println("Found /friends folder");
    }
    if (SD_MMC.exists("/system")) {
        Serial.println("Found /system folder");
    }
}

// --- Query Functions ---
bool audio_can_start_ambient(void)
{
    // Only allow ambient if idle or already playing ambient
    return (g_state == AUDIO_IDLE || g_state == AUDIO_AMBIENT_FORECAST);
}

// --- Playback Functions ---

void audio_play_ambient_forecast(void)
{
    if (!audio_can_start_ambient()) {
        Serial.println("Cannot start ambient: busy");
        return;
    }
    play_file(FILE_AMBIENT, AUDIO_AMBIENT_FORECAST);
}

void audio_play_manual_current(void)
{
    // User tapped home screen: play current weather soundscape
    play_file(FILE_TOUCH_NOW, AUDIO_MANUAL_CURRENT);
}

void audio_play_friend_weather(int friend_id)
{
    const char *file = friend_file_for(friend_id);
    play_file(file, AUDIO_FRIEND_WEATHER);
}

void audio_play_friend_send_confirm(void)
{
    // Short confirmation sound when sending to friend
    play_file(SEND_OK_FILE, AUDIO_NOTIFICATION_PING);
}

void audio_handle_friend_notification(int friend_id)
{
    // Incoming message from friend: play notification
    // Only interrupt if idle or already in ambient mode
    if (g_state == AUDIO_IDLE || g_state == AUDIO_AMBIENT_FORECAST) {
        play_file(NOTIFY_PING_FILE, AUDIO_NOTIFICATION_PING);
    } else {
        Serial.printf("Notification blocked: state=%d\n", g_state);
    }
}

// --- Completion Handler ---
void audio_on_finished(void)
{
    // Called by event manager when audio completes
    Serial.printf("Audio finished: was state=%d\n", g_state);
    g_state = AUDIO_IDLE;
}

// --- Optional: Volume Control ---
void audio_set_volume(uint8_t vol)
{
    if (vol > 21) vol = 21;
    g_volume = vol;
    
    // TODO: Uncomment when audio library is integrated
    // audio.setVolume(g_volume);
    
    Serial.printf("Volume set to %d\n", g_volume);
}

uint8_t audio_get_volume(void)
{
    return g_volume;
}

// --- Optional: Audio Loop (call from main loop if needed) ---
// Some audio libraries need periodic loop() call
void audio_loop(void)
{
    // TODO: Uncomment when audio library is integrated
    // audio.loop();
}
