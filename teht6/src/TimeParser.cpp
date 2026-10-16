#include "TimeParser.h"
#include <cstddef>
#include <cstring>
#include <cctype>
#include <cstdlib>

int parse_time_string(const char *time_str, int *seconds_out) {
    // 1. NULL-osoittimen tarkistus (-5)
    if (time_str == NULL || seconds_out == NULL) {
        return -5;
    }

    // 2. Pituustarkistus: pitää olla 6 merkkiä (-2)
    if (std::strlen(time_str) != 6) {
        return -2;
    }

    // 3. Numeerisuustarkistus: vain numeroita sallittu (-3)
    for (int i = 0; i < 6; i++) {
        if (!std::isdigit(static_cast<unsigned char>(time_str[i]))) {
            return -3;
        }
    }

    // Luetaan tunnit, minuutit ja sekunnit
    char hh_buf[3] = { time_str[0], time_str[1], '\0' };
    char mm_buf[3] = { time_str[2], time_str[3], '\0' };
    char ss_buf[3] = { time_str[4], time_str[5], '\0' };

    int hours = std::atoi(hh_buf);
    int minutes = std::atoi(mm_buf);
    int seconds = std::atoi(ss_buf);

    // 4. Raja-arvotarkistukset (-1)
    if (hours < 0 || hours > 23 ||
        minutes < 0 || minutes > 59 ||
        seconds < 0 || seconds > 59) {
        return -1;
    }

    int total_seconds = (hours * 3600) + (minutes * 60) + seconds;

    // 5. Nolla-ajan tarkistus (-4)
    if (total_seconds == 0) {
        return -4;
    }

    *seconds_out = total_seconds;
    return 0; // OK
}

/* Yhteensopivuusfunktio aiemmallle koodille:
 * Palauttaa lasketut sekunnit jos OK, muuten negatiivisen virhekoodin.
 */
int time_parse(const char *time_str) {
    int seconds = 0;
    int result = parse_time_string(time_str, &seconds);
    if (result == 0) {
        return seconds;
    }
    return result; // Palautetaan negatiivinen virhekoodi (-1...-5)
}