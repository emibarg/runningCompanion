#ifndef EVENTS_H
#define EVENTS_H

#include "stdint.h"

typedef enum {
    EVENT_NONE = 0,
    EVENT_BUTTON_PRESSED,
    EVENT_BUTTON_RELEASED,
    EVENT_STEP_DETECTED,
    EVENT_GPS_UPDATE,
    EVENT_LOGGING_DONE,
} EventType;

void Events_Init(void);
void Events_Push(EventType ev);
EventType Events_Pop(void);
uint8_t Events_HasPending(void);

#endif
