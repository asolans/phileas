#ifndef PHILEAS_CHARS_H
#define PHILEAS_CHARS_H

#include <stddef.h>

/* Caràcters per nom curt o per codi, per a teclats que no els tenen (punt
 * volat, guió llarg, «», ç majúscula…). arg és un nom de la llista
 * (guio-llarg, punt-volat…), un codi hexadecimal («2014», «U+2014», «0x2014»)
 * o un sol caràcter. Escriu el resultat en UTF-8 a out i en retorna els bytes;
 * 0 si no l'entén. */
size_t char_lookup(const char *arg, char *out, size_t size);

/* Llista de noms, separats per espais, per a l'ajuda */
const char *char_names(void);

#endif
