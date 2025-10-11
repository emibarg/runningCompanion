#include "flags.h"

static volatile uint8_t systemFlags = 0;

void Flags_Set(uint8_t flag) {
    systemFlags |= flag;
}

void Flags_Clear(uint8_t flag) {
    systemFlags &= ~flag;
}

uint8_t Flags_IsSet(uint8_t flag) {
    return (systemFlags & flag) != 0;
}

void Flags_ClearAll(void) {
    systemFlags = 0;
}
