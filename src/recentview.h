#ifndef PHILEAS_RECENTVIEW_H
#define PHILEAS_RECENTVIEW_H

#include "editor.h"

/* Documents recents a pantalla completa (l'historial, sense el document
 * obert ni els fitxers que ja no hi són). Retorna 1 i deixa a out el camí
 * triat, o 0 si es cancel·la o la llista és buida. */
int recent_view(Editor *e, char *out, size_t outsz);

#endif
