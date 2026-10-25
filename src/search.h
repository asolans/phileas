#ifndef PHILEAS_SEARCH_H
#define PHILEAS_SEARCH_H

#include "buffer.h"

/* Cerca pat (UTF-8, sense salts de línia) a partir de from.
 *   dir > 0: la primera coincidència que comença a from o després;
 *   dir < 0: l'última que comença abans de from.
 * Si arriba a un extrem del document, continua per l'altre i posa
 * *wrapped = 1. Si pat no té cap majúscula, no distingeix majúscules de
 * minúscules. Retorna 1 i el tram [*a, *z) si en troba. */
int search_find(const Buffer *b, Pos from, const char *pat, int dir,
                Pos *a, Pos *z, int *wrapped);

#endif
