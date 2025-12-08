#ifndef EVENT_MANAGER_H
#define EVENT_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    EVT_TIMER_30MIN,
    EVT_TOUCH_CLICK,
    EVT_ENCODER_ROTATE,
    EVT_ENCODER_CLICK,
    EVT_FRIEND_MESSAGE,
    EVT_AUDIO_COMPLETE
} event_type_t;

typedef enum {
    EV_PRIO_CRITICAL = 0,
    EV_PRIO_HIGH     = 1,
    EV_PRIO_LOW      = 2
} event_prio_t;

typedef struct {
    event_type_t  type;
    event_prio_t  prio;
    int32_t       i32;
    void         *ptr;
    uint32_t      ts_ms;
} system_event_t;

void event_manager_init(void);
bool event_post(event_type_t type, event_prio_t prio, int32_t i32, void *ptr);
void event_process_once(void);
void event_notify_audio_complete(void);

#ifdef __cplusplus
}
#endif

#endif
