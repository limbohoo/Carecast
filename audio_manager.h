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

void audio_manager_init(void);

void audio_play_ambient_forecast(void);
void audio_play_manual_current(void);
void audio_play_friend_weather(int friend_id);
void audio_play_friend_send_confirm(void);
void audio_handle_friend_notification(int friend_id);

bool audio_can_start_ambient(void);
void audio_on_finished(void);

#endif
