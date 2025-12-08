#include "event_manager.h"
#include "audio_manager.h"
#include "lvgl.h"

#define EVENT_QUEUE_SIZE 16

static system_event_t q[EVENT_QUEUE_SIZE];
static uint8_t q_head = 0, q_tail = 0;

static bool queue_empty() { return q_head == q_tail; }
static bool queue_full()  { return (uint8_t)((q_tail + 1) % EVENT_QUEUE_SIZE) == q_head; }

void event_manager_init(void)
{
    q_head = q_tail = 0;
}

bool event_post(event_type_t type, event_prio_t prio, int32_t i32, void *ptr)
{
    if (queue_full()) return false;

    system_event_t ev;
    ev.type  = type;
    ev.prio  = prio;
    ev.i32   = i32;
    ev.ptr   = ptr;
    ev.ts_ms = lv_tick_get();

    if (prio == EV_PRIO_CRITICAL) {
        // insert at head
        q_head = (uint8_t)((q_head + EVENT_QUEUE_SIZE - 1) % EVENT_QUEUE_SIZE);
        q[q_head] = ev;
    } else {
        q[q_tail] = ev;
        q_tail = (uint8_t)((q_tail + 1) % EVENT_QUEUE_SIZE);
    }
    return true;
}

static bool queue_pop(system_event_t *ev)
{
    if (queue_empty()) return false;
    *ev = q[q_head];
    q_head = (uint8_t)((q_head + 1) % EVENT_QUEUE_SIZE);
    return true;
}

void event_notify_audio_complete(void)
{
    event_post(EVT_AUDIO_COMPLETE, EV_PRIO_LOW, 0, NULL);
}

void event_process_once(void)
{
    system_event_t ev;
    if (!queue_pop(&ev)) return;

    switch (ev.type) {
    case EVT_TOUCH_CLICK:
        audio_play_manual_current();
        break;

    case EVT_ENCODER_ROTATE:
        // currently no direct audio effect; UI already handled in knob.c
        (void)ev;
        break;

    case EVT_ENCODER_CLICK:
        // ev.i32 = friend id
        audio_play_friend_send_confirm();
        audio_play_friend_weather(ev.i32);
        break;

    case EVT_TIMER_30MIN:
        if (audio_can_start_ambient()) {
            audio_play_ambient_forecast();
        }
        break;

    case EVT_FRIEND_MESSAGE:
        // ptr could encode friend id if needed
        audio_handle_friend_notification((int)(intptr_t)ev.ptr);
        break;

    case EVT_AUDIO_COMPLETE:
        audio_on_finished();
        break;
    }
}
