#ifndef COMU_YAML_H
#define COMU_YAML_H

#include <stddef.h>

/* YAML mínim de la capçalera d'un fitxer: només claus planes (`clau: valor`)
 * i llistes en línia (`[a, b]`). La capçalera és al principi del fitxer, entre
 * una línia `---` i una altra `---` (o `...`). Tot el que no entenem
 * (comentaris, claus niades, línies sagnades) es respecta tal qual. */

/* Valor de la clau (sense cometes), copiat a out. Retorna 1 si hi és, 0 si no
 * hi ha capçalera o la clau. Si es repeteix, val la primera. */
int yaml_get(const char *text, size_t len, const char *key, char *out, size_t outsz);

/* Parteix un valor `[a, b, "c d"]` en elements (cadenes noves). Retorna el
 * nombre d'elements (0 si no és una llista); *items s'ha d'alliberar amb
 * yaml_list_free. */
size_t yaml_list(const char *value, char ***items);
void   yaml_list_free(char **items, size_t n);

/* Text nou amb la clau canviada: només es reescriu la seva línia (o
 * s'afegeix al final de la capçalera; o es crea la capçalera si no n'hi ha).
 * Amb raw=0 el valor es posa entre cometes si cal; amb raw=1 es copia tal qual
 * (p. ex. una llista). El resultat és malloc, de longitud *outlen. */
char *yaml_set(const char *text, size_t len, const char *key, const char *value,
               int raw, size_t *outlen);

#endif
