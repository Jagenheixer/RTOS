#ifndef TIME_PARSER_H
#define TIME_PARSER_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Parsii aikamerkkijonon muotoa "HHMMSS".
 * 
 * Parametrit:
 *   time_str - osoitin aikamerkkijonoon (esim. "000120")
 *   seconds_out - osoitin muuttujaan, johon laskettu sekuntimäärä tallennetuu
 * 
 * Palautusarvot:
 *    0 = OK (sekunnit tallennettu seconds_out-muuttujaan)
 *   -1 = Raja-arvirhe (tunnit > 23, minuutit > 59 tai sekunnit > 59)
 *   -2 = Pituusvirhe (merkkijono ei ole tasan 6 merkkiä)
 *   -3 = Tyyppivirhe (sisältää muita kuin numeroita)
 *   -4 = Nolla-aika (000000 ei sallittu)
 *   -5 = NULL-osoitin
 */
int parse_time_string(const char *time_str, int *seconds_out);

/* Yhteensopivuusfunktio aiempaan koodiin */
int time_parse(const char *time_str);

#ifdef __cplusplus
}
#endif

#endif // TIME_PARSER_H