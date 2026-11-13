#ifndef COMU_FOLD_H
#define COMU_FOLD_H

#include <stddef.h>

/* Normalització de text per comparar (codi comú de Recerca; Fàbula s'hi
 * passarà més endavant). No depèn del locale: les minúscules de l'alfabet
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

#endif
