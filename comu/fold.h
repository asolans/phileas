#ifndef COMU_FOLD_H
#define COMU_FOLD_H

#include <stddef.h>
#include <wchar.h>

/* Normalització de text per comparar (codi comú de Recerca i Fàbula). No depèn del locale: les minúscules de l'alfabet
 * llatí es fan aquí mateix.
 *
 * cm_lower: només minúscules. És la comparació d'Obsidian per resoldre
 *           [[enllaços]] (no distingeix majúscules, però sí accents).
 * cm_fold:  minúscules i sense accents (à→a, ç→c, l·l→ll, æ→ae…). És la de
 *           les cerques i els filtres.
 * Retornen una cadena nova (malloc). Els bytes que no són UTF-8 vàlid es
 * copien tal qual. */
char *cm_lower(const char *s, size_t len);
char *cm_fold(const char *s, size_t len);

/* Les mateixes regles per a un sol caràcter (per a qui ha de saber d'on ve
 * cada byte, com la cerca de Fàbula): cm_lower_char en fa la minúscula, i
 * cm_strip_char dona l'equivalent sense accents d'una minúscula («a», «ae»,
 * «» per al punt volat…), o NULL si es queda igual. */
wchar_t     cm_lower_char(wchar_t c);
const char *cm_strip_char(wchar_t c);

#endif
