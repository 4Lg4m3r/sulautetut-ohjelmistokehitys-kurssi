#include <string.h>
#include <ctype.h>
#include "TimeParser.h"

int time_parse(char *time)
{
    // NULL check
    if (time == NULL) {
        return TIME_ARRAY_ERROR;
    }

    // Must be exactly HHMMSS
    if (strlen(time) != 6) {
        return TIME_LEN_ERROR;
    }

    // Every character must be a number
    for (int i = 0; i < 6; i++) {
        if (!isdigit((unsigned char)time[i])) {
            return TIME_VALUE_ERROR;
        }
    }

    // Parse HHMMSS
    int hours =
        (time[0] - '0') * 10 +
        (time[1] - '0');

    int minutes =
        (time[2] - '0') * 10 +
        (time[3] - '0');

    int seconds =
        (time[4] - '0') * 10 +
        (time[5] - '0');

    // Validate values
    if (hours > 23) {
        return TIME_VALUE_ERROR;
    }

    if (minutes > 59) {
        return TIME_VALUE_ERROR;
    }

    if (seconds > 59) {
        return TIME_VALUE_ERROR;
    }

    // 00:00:00 is not allowed
    if (hours == 0 &&
        minutes == 0 &&
        seconds == 0) {
        return TIME_VALUE_ERROR;
    }

    // Convert everything to seconds
    int total_seconds =
        hours * 3600 +
        minutes * 60 +
        seconds;

    return total_seconds;
}