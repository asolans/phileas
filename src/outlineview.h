#ifndef PHILEAS_OUTLINEVIEW_H
#define PHILEAS_OUTLINEVIEW_H

#include "editor.h"

/* Esquema a pantalla completa: les capçaleres del document, sagnades pel
 * nivell, amb les paraules de cada secció. Comença a la secció del cursor.
 * Retorna 1 i deixa a *line la línia triada, o 0 si es cancel·la. */
int outline_view(Editor *e, size_t *line);

#endif
