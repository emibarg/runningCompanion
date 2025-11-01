#include "gps.h"
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

static char gpsBufferA[GPS_BUFFER_SIZE];
static char gpsBufferB[GPS_BUFFER_SIZE];

static char *activeBuffer = gpsBufferA;
static char *readyBuffer  = NULL;

static uint16_t writeIndex = 0;
static int bufferFull = 0;
static bool gpsReady = false;

static char nmeaBuffer[GPS_LINE_MAX];
static uint8_t nmeaIndex = 0;
static uint32_t lastSaveTime = 0;

static int gpsValidate(const char *nmea);
static void saveToActiveBuffer(const char *line);

/**
 * Simple checksum validator for NMEA.
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
    sscanf(&nmea[i + 1], "%2X", &received);
    return (checksum == received);
}

/**
 * Append a validated line to the active buffer.
 */
static void saveToActiveBuffer(const char *line) {
    uint16_t len = strlen(line);
    if (len + writeIndex + 2 < GPS_BUFFER_SIZE) {
        memcpy(&activeBuffer[writeIndex], line, len);
        writeIndex += len;
        activeBuffer[writeIndex++] = '\r';
        activeBuffer[writeIndex++] = '\n';
    } else {
        // When full, swap buffers
        readyBuffer = activeBuffer;
        activeBuffer = (activeBuffer == gpsBufferA) ? gpsBufferB : gpsBufferA;
        writeIndex = 0;
        memset(activeBuffer, 0, GPS_BUFFER_SIZE);
        bufferFull = 1;
    }
}

/**
 * Process incoming GPS bytes (works exactly as before).
 */
static char lastUtcTime[16] = "--:--:--";
static char lastUtcDate[16] = "----/--/--";
static bool gpsValidFix = false;

void gpsProcessByte(uint8_t byte) {
    if (byte != '\n' && nmeaIndex < GPS_LINE_MAX - 1) {
        nmeaBuffer[nmeaIndex++] = byte;
    } else {
        nmeaBuffer[nmeaIndex] = '\0';
        nmeaIndex = 0;

        if (!gpsValidate(nmeaBuffer))
            return;

        // === Handle GPGGA ===
        if (!strncmp(nmeaBuffer, "$GPGGA", 6)) {
            gpsReady = true;
            uint32_t now = HAL_GetTick();
            if (now - lastSaveTime >= 5000) {
                saveToActiveBuffer(nmeaBuffer);
                lastSaveTime = now;
            }

            // Extract UTC time (hhmmss)
            char utc[16];
            if (sscanf(nmeaBuffer, "$GPGGA,%[^,],", utc) == 1 && strlen(utc) >= 6) {
                snprintf(lastUtcTime, sizeof(lastUtcTime),
                         "%c%c:%c%c:%c%c",
                         utc[0], utc[1], utc[2], utc[3], utc[4], utc[5]);
            }
        }

        // === Handle GPRMC (contains both time + date) ===
        if (!strncmp(nmeaBuffer, "$GPRMC", 6)) {
            char utc[16], status, date[16];
            if (sscanf(nmeaBuffer,
                       "$GPRMC,%[^,],%c,%*[^,],%*[^,],%*[^,],%*[^,],%*[^,],%*[^,],%[^,]",
                       utc, &status, date) == 3) {

                if (status == 'A') {  // Active fix
                    gpsValidFix = true;

                    if (strlen(utc) >= 6) {
                        snprintf(lastUtcTime, sizeof(lastUtcTime),
                                 "%c%c:%c%c:%c%c",
                                 utc[0], utc[1], utc[2], utc[3], utc[4], utc[5]);
                    }

                    if (strlen(date) == 6) {
                        snprintf(lastUtcDate, sizeof(lastUtcDate),
                                 "20%c%c-%c%c-%c%c",
                                 date[4], date[5], date[2], date[3], date[0], date[1]);
                    }
                } else {
                    gpsValidFix = false; // no fix
                }
            }
    }
    }
}


/**
 * API for system_state.c
 */

bool gpsHasValidFix(void) {
        return gpsValidFix;
    }


const char* gpsGetLastDate(void) {
    return lastUtcDate;
}

const char* gpsGetLastTime(void) {
    return lastUtcTime;
}
int gpsIsBufferFull(void) {
    return bufferFull;
}

bool gpsIsReady(void) {
    return gpsReady;
}

int gpsHasReadyBuffer(void) {
    return (readyBuffer != NULL);
}

const char* gpsGetReadyBuffer(void) {
    return readyBuffer ? readyBuffer : activeBuffer;
}

void gpsMarkBufferWritten(void) {
    readyBuffer = NULL;
    bufferFull = 0;
}

void gpsClearBuffer(void) {
    memset(activeBuffer, 0, GPS_BUFFER_SIZE);
    writeIndex = 0;
    bufferFull = 0;
}


// Convert NMEA "DDMM.MMMMM..." or "DDDMM.MMMMM..." into "-DD.DDDDD" string (5 decimals).
// All integer math, no floats. outStr must be at least 16 bytes.
void nmeaToDecimalDegreesString(const char* nmea, char dir, char* outStr) {
    if (!nmea || !nmea[0]) {
        strcpy(outStr, "0.00000");
        return;
    }

    // find dot
    const char *dot = strchr(nmea, '.');
    int nlen = strlen(nmea);

    // dot might be NULL (no fractional part) — handle gracefully
    int dotIndex = dot ? (int)(dot - nmea) : nlen;

    // determine degree digits: lat = 2, lon = 3
    int degDigits = (dotIndex > 4) ? 3 : 2; // if like 12345.x --> degDigits=3

    // basic sanity checks
    if (dotIndex <= degDigits) {
        strcpy(outStr, "0.00000");
        return;
    }

    // parse degrees (first degDigits chars)
    char degBuf[4] = {0};
    for (int i = 0; i < degDigits; ++i) degBuf[i] = nmea[i];
    int degrees = atoi(degBuf);

    // parse minutes integer part (the mm before the dot)
    // minutes integer digits count = dotIndex - degDigits
    int minIntLen = dotIndex - degDigits;
    char minIntBuf[8] = {0};
    for (int i = 0; i < minIntLen && i < (int)sizeof(minIntBuf)-1; ++i)
        minIntBuf[i] = nmea[degDigits + i];
    int minutes_int = atoi(minIntBuf); // e.g., "58" -> 58

    // parse fractional minutes (digits after dot) up to, say, 6 digits for safety
    char minFracBuf[8] = {0};
    int fracLen = 0;
    if (dot) {
        const char *p = dot + 1;
        while (*p && fracLen < 6) { // capture up to 6 digits
            if (*p < '0' || *p > '9') break;
            minFracBuf[fracLen++] = *p++;
        }
    }
    // scale frac to exactly 5 digits (we want minutes * 100000 / 60 later)
    // build minutesTimes100000 = (minutes_int + frac/10^fracLen) * 100000
    // we'll compute minutesTimes100000 = minutes_int * 100000 + frac_scaled
    int fracScaled = 0;
    if (fracLen > 0) {
        // convert fractional string to integer
        int fracVal = 0;
        for (int i = 0; i < fracLen; ++i) fracVal = fracVal * 10 + (minFracBuf[i] - '0');
        // bring fracVal to 5 digits: fracScaled = fracVal * 10^(5 - fracLen)
        int pad = 5 - fracLen;
        if (pad >= 0) {
            for (int i = 0; i < pad; ++i) fracVal *= 10;
            fracScaled = fracVal;
        } else {
            // if more than 5 digits, truncate (divide)
            for (int i = 0; i < -pad; ++i) fracVal /= 10;
            fracScaled = fracVal;
        }
    }

    // minutesTimes100000 (minutes * 100000)
    // minutes_int * 100000 + fracScaled
    long minutesTimes100000 = (long)minutes_int * 100000L + (long)fracScaled;

    // Now convert to decimal degrees scaled by 100000:
    // decimalDegX100000 = degrees * 100000 + (minutes * 100000) / 60
    // minutesTimes100000/60 uses integer division (good enough)
    long decimalDegX100000 = (long)degrees * 100000L + (minutesTimes100000 / 60L);

    if (dir == 'S' || dir == 'W') decimalDegX100000 = -decimalDegX100000;

    // format into string with decimal point before last 5 digits
    // e.g. decimalDegX100000 = -3098150 -> "-30.98150"
    long v = decimalDegX100000;
    char tmp[32];
    // store absolute value digits in tmpSignless
    if (v < 0) v = -v;
    // write digits into a signless buffer
    // tmpSignless will contain the digits without sign
    char tmpSignless[32];
    int pos = 0;
    // convert v to string (reverse)
    long t = v;
    if (t == 0) {
        tmpSignless[pos++] = '0';
    } else {
        char rev[32];
        int r = 0;
        while (t > 0) {
            rev[r++] = '0' + (t % 10);
            t /= 10;
        }
        // reverse into tmpSignless
        for (int i = r - 1; i >= 0; --i) tmpSignless[pos++] = rev[i];
    }
    tmpSignless[pos] = '\0';

    // ensure there are at least 6 digits (so we can place dot for .#####)
    int len = strlen(tmpSignless);
    if (len <= 5) {
        // pad left with zeros to ensure at least 6 digits
        char padded[32];
        int need = 6 - len;
        int k = 0;
        for (; k < need; ++k) padded[k] = '0';
        strcpy(padded + k, tmpSignless);
        strcpy(tmpSignless, padded);
        len = strlen(tmpSignless);
    }

    // build output
    char *outp = outStr;
    if (decimalDegX100000 < 0) *outp++ = '-';

    int dotPosition = len - 5; // position in tmpSignless where '.' goes
    for (int i = 0; i < len; ++i) {
        if (i == dotPosition) *outp++ = '.';
        *outp++ = tmpSignless[i];
    }
    *outp = '\0';
}

void gpsFormatBuffer(char *outBuffer, size_t outSize, const char *input) {
    outBuffer[0] = '\0';
    const char *lineStart = input;
    const char *lineEnd;
    size_t pos = 0;

    while ((lineEnd = strchr(lineStart, '\n')) != NULL) {
        size_t len = lineEnd - lineStart;
        if (len > 0 && len < GPS_LINE_MAX) {
            char line[GPS_LINE_MAX];
            memcpy(line, lineStart, len);
            line[len] = '\0';

            // Only process $GPGGA
            if (!strncmp(line, "$GPGGA", 6)) {
                char utc[12] = "", lat[16] = "", ns[2] = "", lon[16] = "", ew[2] = "";
                int matched = sscanf(line, "$GPGGA,%[^,],%[^,],%[^,],%[^,],%[^,]", utc, lat, ns, lon, ew);
                if (matched == 5) {
                    // Convert lat/lon to decimal degrees
                    char latStr[16], lonStr[16];
                    nmeaToDecimalDegreesString(lat, ns[0], latStr);
                    nmeaToDecimalDegreesString(lon, ew[0], lonStr);

                    // Format UTC time with colons
                    char formattedUtc[16];
                    if (strlen(utc) >= 6)
                        snprintf(formattedUtc, sizeof(formattedUtc), "%c%c:%c%c:%c%c",
                                 utc[0], utc[1], utc[2], utc[3], utc[4], utc[5]);
                    else
                        strcpy(formattedUtc, "00:00:00");

                    // Append formatted line
                    int written = snprintf(outBuffer + pos, outSize - pos, "%s,%s,%s\r\n",
                                           formattedUtc, latStr, lonStr);
                    if (written < 0 || (size_t)written >= outSize - pos)
                        break; // Prevent overflow
                    pos += written;
                }
            }
        }
        lineStart = lineEnd + 1;
    }
}



/**
 * Force the active buffer to become the ready buffer,
 * even if it's not full.
 */
void gpsForceReadyBuffer(void) {
    if (writeIndex > 0) {
        readyBuffer = activeBuffer;
        activeBuffer = (activeBuffer == gpsBufferA) ? gpsBufferB : gpsBufferA;
        memset(activeBuffer, 0, GPS_BUFFER_SIZE);
        writeIndex = 0;
        bufferFull = 1;
    }
}

