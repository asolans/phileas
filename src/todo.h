#ifndef PHILEAS_TODO_H
#define PHILEAS_TODO_H

#include "buffer.h"

#include <stddef.h>

/* Marcadors de pendents dins del text: TODO (paraula sencera, en majúscules),
 * [?] i %%comentari%% (tancat a la mateixa línia). No compten dins del codi
 * (en línia, blocs entre tanques o sagnats) ni de les metadades YAML. */

/* Primer marcador de la línia (de tipus kind) que comença a partir de from:
 * deixa a *a i *b els límits en bytes i retorna 1; 0 si no n'hi ha més. */
int todo_next(const char *s, size_t len, int kind, size_t from, size_t *a, size_t *b);

/* Línies amb algun marcador, en ordre, a *lines (cal alliberar-lo amb free).
 * Retorna quantes. En text pla, cap. */
size_t todo_collect(Buffer *b, size_t **lines);

#endif
