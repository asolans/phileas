#ifndef PHILEAS_CALAIXVIEW_H
#define PHILEAS_CALAIXVIEW_H

#include "editor.h"

/* Les entrades del calaix a pantalla completa, filtrables per etiqueta o
 * text. Retorna 1 i deixa a line la línia (des d'1) de l'entrada triada, o 0 si
 * es cancel·la o el calaix és buit. */
int calaix_view(Editor *e, size_t *line);

#endif
