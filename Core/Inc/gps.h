/*
 * gps.h
 *
 *  Created on: Sep 2, 2025
 *      Author: emi
 */

#ifndef INC_GPS_H_
#define INC_GPS_H_

#include "main.h"

#define GPS_BUFFER_SIZE  1024   // total buffer for storing GPGGA lines
#define GPS_LINE_MAX     128    // max length of a single NMEA line

void gpsProcessByte(uint8_t byte);

const char* gpsGetBuffer(void);
uint16_t gpsGetBufferLength(void);
void gpsClearBuffer(void);
int gpsIsBufferFull(void);

#endif /* INC_GPS_H_ */
