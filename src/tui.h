#ifndef PHILEAS_TUI_H
#define PHILEAS_TUI_H

/* ncurses amb suport de caràcters amples (UTF-8) */
#define NCURSES_WIDECHAR 1
#include <curses.h>

/* Atributs dels elements de la interfície segons el tema (render.c): colors
 * propis o, si no en tenen, vídeo invers (barra i selecció) o res (menús) */
enum { UI_BAR, UI_SEL, UI_MENU, UI_N };
attr_t ui_attr(int role);

#endif
