#ifndef PHILEAS_CALAIXVIEW_H
#define PHILEAS_CALAIXVIEW_H

#include "editor.h"

/* Les entrades del calaix a pantalla completa, filtrables per etiqueta o
 * text. Retorna 1 i deixa a out la capçalera triada (sense el salt de línia),
 * o 0 si es cancel·la o el calaix és buit. */
int calaix_view(Editor *e, char *out, size_t outsz);

#endif
