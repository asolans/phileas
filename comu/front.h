#ifndef COMU_FRONT_H
#define COMU_FRONT_H

#include <stddef.h>

/* Lectura de la capçalera YAML d'una nota d'Obsidian (les «propietats").
 * Entén el que escriu Obsidian:
 *
 *   clau: valor          clau: "valor"        clau: 'valor'
 *   clau: [a, "b, c"]    llista en línia
 *   clau:                llista en bloc
 *     - a
 *     - "[[Nota]]"
 *
 * El que no entén (mapes niats, text en diverses línies) se salta. */

/* Una propietat. index = -1 per a un valor simple; 0, 1… per als elements
 * d'una llista (en línia o en bloc). Els valors ja no porten cometes. */
typedef void (*cm_prop_fn)(const char *key, size_t klen, const char *val, size_t vlen,
                           int index, void *ud);

/* Recorre la capçalera de text[0..len) i retorna on comença el cos (0 si no
 * n'hi ha). *body_line rep el número de línia del cos (1 = la primera línia
 * del fitxer). fn pot ser NULL. */
size_t cm_front(const char *text, size_t len, size_t *body_line, cm_prop_fn fn, void *ud);

#endif
