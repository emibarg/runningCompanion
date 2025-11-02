#ifndef INC_GPS_H_
#define INC_GPS_H_

#include "main.h"
#include <stdbool.h>

#define GPS_BUFFER_SIZE 1024
#define GPS_LINE_MAX    128

void gpsProcessByte(uint8_t byte);

const char* gpsGetReadyBuffer(void);
int gpsHasReadyBuffer(void);
void gpsMarkBufferWritten(void);

bool gpsIsReady(void);
int gpsIsBufferFull(void);
void gpsClearBuffer(void);
void nmeaToDecimalDegreesString(const char* nmea, char dir, char* outStr);
void gpsFormatBuffer(char *outBuffer, size_t outSize, const char *input);
void gpsForceReadyBuffer(void);
const char* gpsGetLastDate(void);
const char* gpsGetLastTime(void);
bool gpsHasValidFix(void);
const char* gpsGetLastVelocity(void);
void gpsConvertUtcToLocal(const char* utc, char* outLocal, int offsetHours);


#endif
