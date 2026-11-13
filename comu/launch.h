#ifndef COMU_LAUNCH_H
#define COMU_LAUNCH_H

#include <stddef.h>

/* L'editor de l'opció «editor» del phileasrc (ordre i arguments separats per
 * espais); NULL o "" = phileas. FABULA_EDITOR, si hi és, hi mana. */
void launch_set_editor(const char *cmd);

/* Executa l'editor (FABULA_EDITOR, o el de launch_set_editor, o phileas)
 * sobre el fitxer, amb el cursor al paràgraf `line`
 * (1 = el primer; 0 = on era). Deixa el terminal a l'editor i el torna en
 * acabar. Retorna 0, o -1 amb el motiu a err (no s'ha pogut executar, o
 * l'editor ha acabat amb error). */
int launch_editor(const char *path, size_t line, char *err, size_t errsz);

/* Executa un programa de pantalla sencera (argv acabat en NULL; argv[0] es
 * busca al PATH), deixant-li el terminal fins que acaba. 0, o -1 amb el
 * motiu a err. */
int launch_program(char *const argv[], char *err, size_t errsz);

#endif
