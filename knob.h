#ifndef _KNOB_H
#define _KNOB_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include "bidi_switch_knob.h"   // brings in knob_event_t, KNOB_LEFT, KNOB_RIGHT, etc.

/* Core UI initialization */
void knob_init_system(void);

/* Rotary encoder logic entry (high-level) */
void knob_change(knob_event_t k, int cont);

/* Weather data injection */
void update_my_weather(int temp, const char* name);
void update_friend_weather(int id, int temp, const char* name);

#ifdef __cplusplus
}
#endif

#endif
