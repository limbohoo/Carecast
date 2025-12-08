#ifndef AUDIO_MANAGER_H
#define AUDIO_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    AUDIO_IDLE = 0,
    AUDIO_AMBIENT_FORECAST,
    AUDIO_MANUAL_CURRENT,
    AUDIO_FRIEND_WEATHER,
    AUDIO_NOTIFICATION_PING
} audio_state_t;

// --- Init ---
void audio_manager_init(void);

// --- Playback Control ---
void audio_play_ambient_forecast(void);      // 30-min scheduled soundscape
void audio_play_manual_current(void);        // User tap on home
void audio_play_friend_weather(int friend_id); // Share weather to friend
void audio_play_friend_send_confirm(void);   // Quick "sent" sound
void audio_handle_friend_notification(int friend_id); // Incoming message

// --- State Query ---
bool audio_can_start_ambient(void);

// --- Completion ---
void audio_on_finished(void);

// --- Volume Control (optional) ---
void audio_set_volume(uint8_t vol);  // 0-21
uint8_t audio_get_volume(void);

// --- Loop (optional, call from main loop if needed) ---
void audio_loop(void);

#endif
