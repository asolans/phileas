#ifndef PHILEAS_COMPLETE_H
#define PHILEAS_COMPLETE_H

#include <stddef.h>

/* Autocompleció d'un nom d'entre una llista (la paleta d'ordres): allarga
 * word fins al prefix comú més llarg dels noms que comencen per word.
 * Retorna quants noms hi coincideixen. */
size_t complete_word(char *word, size_t size, const char *const *names, size_t n);

#endif
