#ifndef PHILEAS_PAIRS_H
#define PHILEAS_PAIRS_H

#include <stddef.h>

/* Comprova els aparellaments d'un paràgraf: «…», (…), […], “…”, "…" i el guió
 * de diàleg (—Hola —va dir— i ja). Cada paràgraf s'ha de tancar tot sol.
 * Retorna 1 amb la posició (byte) i el motiu si hi ha un problema, o 0.
 * El codi en línia (`…`) no es revisa; tampoc els numerals de llista («1)») ni
 * els emoticones (:) ;)). */
int pairs_check(const char *s, size_t len, size_t *pos, const char **why);

#endif
