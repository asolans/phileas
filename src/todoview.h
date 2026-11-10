#ifndef PHILEAS_TODOVIEW_H
#define PHILEAS_TODOVIEW_H

#include "editor.h"

/* Llista a pantalla completa de les línies amb marcadors de pendents
 * (TODO, [?], %%nota%%), amb el número de línia. Comença al primer
 * pendent des del cursor. Retorna 1 i deixa a *line la línia triada, o 0
 * si es cancel·la o no n'hi ha cap. */
int todo_view(Editor *e, size_t *line);

#endif
