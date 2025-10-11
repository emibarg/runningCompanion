#ifndef SYSTEM_STATE_H
#define SYSTEM_STATE_H

#include "main.h"
#include "gps.h"
#include "mpu6050.h"
#include "fatfs.h"
#include "st7735.h"

typedef enum {
    STATE_INIT = 0,
    STATE_IDLE,
	STATE_RUNNING,
    STATE_MPU_READ,
    STATE_GPS_PROCESS,
    STATE_SD_WRITE,
    STATE_ERROR
} SystemState_t;

void systemState_Init(void);
void systemState_Run(void);

#endif
