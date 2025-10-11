#ifndef FLAGS_H
#define FLAGS_H

#include "stdint.h"

typedef enum {
    FLAG_BUTTON_PRESSED = 1 << 0,
    FLAG_GPS_FIX        = 1 << 1,
    FLAG_LOGGING_ACTIVE = 1 << 2,
    FLAG_STEP_MODE      = 1 << 3,
} FlagBits;

void Flags_Set(uint8_t flag);
void Flags_Clear(uint8_t flag);
uint8_t Flags_IsSet(uint8_t flag);
void Flags_ClearAll(void);

#endif
