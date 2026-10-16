#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "TimeParser.h"

// time format: HHMMSS (6 characters)
int time_parse(char *time) {

    // Default return value
    int seconds = TIME_LEN_ERROR;

    // 1. Tarkista ettei osoitin ole NULL
    if (time == NULL) {
        return TIME_NULL_ERROR;
    }

    // 2. Tarkista pituus (pitää olla tasan 6 merkkiä)
    if (strlen(time) != 6) {
        return TIME_LEN_ERROR;
    }

    // 3. Tarkistus että kaikki merkit ovat numeroita
    for (int i = 0; i < 6; i++) {
        if (!isdigit((unsigned char)time[i])) {
            return TIME_DIGIT_ERROR;
        }
    }

    // Parse values from time string
    // For example: 124033 -> 12hour 40min 33sec
    int values[3];
    values[2] = atoi(time + 4); // seconds
    time[4] = 0;
    values[1] = atoi(time + 2); // minutes
    time[2] = 0;
    values[0] = atoi(time);     // hours

    // 4. Boundary check (tunnit 0-23, minuutit 0-59, sekunnit 0-59)
    if (values[0] < 0 || values[0] > 23 ||
        values[1] < 0 || values[1] > 59 ||
        values[2] < 0 || values[2] > 59) {
        return TIME_VALUE_ERROR;
    }

    // 5. Lasketut sekunnit (minuutit * 60 + sekunnit)
    seconds = (values[1] * 60) + values[2];

    // 6. Tarkistus ettei aika ole 0 sekuntia
    if (seconds == 0) {
        return TIME_ZERO_ERROR;
    }

    return seconds;
}