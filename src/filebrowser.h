#ifndef PHILEAS_FILEBROWSER_H
#define PHILEAS_FILEBROWSER_H

#include "editor.h"

/* Navegador de fitxers a pantalla completa. Comença al directori del
 * document actual (o a l'últim visitat, o al de treball). Retorna 1 i deixa
 * a out el camí triat, que pot no existir (fitxer nou), o 0 si es cancel·la. */
int file_browser(Editor *e, char *out, size_t outsz);

#endif
