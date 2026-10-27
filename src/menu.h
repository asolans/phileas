#ifndef PHILEAS_MENU_H
#define PHILEAS_MENU_H

#include "command.h"

/* Menú a l'estil de WordGrinder (Esc): menú principal i submenús, amb la
 * drecera de cada ordre. Retorna l'ordre triada, i a *args els seus
 * arguments (NULL si no en té), o NULL si es tanca sense triar res. No
 * l'executa: ho fa qui crida, un cop el menú ja no és a la pantalla. */
const Command *menu_choose(Editor *e, const char **args);

#endif
