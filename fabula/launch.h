#ifndef FABULA_LAUNCH_H
#define FABULA_LAUNCH_H

#include <stddef.h>

/* Executa l'editor (Phileas, o el que digui FABULA_EDITOR: ordre i arguments
 * separats per espais) sobre el fitxer, amb el cursor al paràgraf `line`
 * (1 = el primer; 0 = on era). Deixa el terminal a l'editor i el torna en
 * acabar. Retorna 0, o -1 amb el motiu a err (no s'ha pogut executar, o
 * l'editor ha acabat amb error). */
int launch_editor(const char *path, size_t line, char *err, size_t errsz);

#endif
