/*
 * gps.c
 *
 *  Created on: Sep 2, 2025
 *      Author: emi
 */

#include "gps.h"
#include <string.h>
#include <stdio.h>


static char gpsBuffer[GPS_BUFFER_SIZE];
static uint16_t gpsWriteIndex = 0;
static int bufferFull = 0;

static char nmeaBuffer[GPS_LINE_MAX];
static uint8_t nmeaIndex = 0;

static uint32_t lastSaveTime = 0;

/**
 * Append a validated GPGGA line into the gpsBuffer.
 */
static void saveToBuffer(const char *line) {
    uint16_t len = strlen(line);

    if (len + gpsWriteIndex + 2 < GPS_BUFFER_SIZE) { // +2 for CRLF
        memcpy(&gpsBuffer[gpsWriteIndex], line, len);
        gpsWriteIndex += len;
        gpsBuffer[gpsWriteIndex++] = '\r';
        gpsBuffer[gpsWriteIndex++] = '\n';
    } else {
        bufferFull = 1;  // buffer is full, stop writing
    }
}


/**
 * Simple NMEA checksum validator.
 */
static int gpsValidate(const char *nmea) {
    if (nmea[0] != '$') return 0;

    int checksum = 0;
    int i = 1;
    while (nmea[i] && nmea[i] != '*' && i < 75) {
        checksum ^= nmea[i];
        i++;
    }

    if (nmea[i] != '*') return 0;

    int received;
    sscanf(&nmea[i+1], "%2X", &received);

    return (checksum == received);
}


/**
 * Process incoming GPS bytes, one at a time.
 */
void gpsProcessByte(uint8_t byte) {
    if (byte != '\n' && nmeaIndex < GPS_LINE_MAX - 1) {
        nmeaBuffer[nmeaIndex++] = byte;
    } else {
        nmeaBuffer[nmeaIndex] = '\0';
        nmeaIndex = 0;

        if (gpsValidate(nmeaBuffer) && !strncmp(nmeaBuffer, "$GPGGA", 6)) {
            uint32_t now = HAL_GetTick();
            if (now - lastSaveTime >= 5000) {
                saveToBuffer(nmeaBuffer);
                lastSaveTime = now;
            }
        }
    }
}


/**
 * Expose buffer for SD writing later.
 */
const char* gpsGetBuffer(void) {
    return gpsBuffer;
}

uint16_t gpsGetBufferLength(void) {
    return gpsWriteIndex;
}

void gpsClearBuffer(void) {
    gpsWriteIndex = 0;
}

int gpsIsBufferFull(void) {
    return bufferFull;
}
