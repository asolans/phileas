#ifndef FABULA_FTERM_H
#define FABULA_FTERM_H

#define NCURSES_WIDECHAR 1
#include <curses.h>

#include "theme.h"

/* Terminal de Fàbula: ncurses, colors del tema (el mateix de Phileas) i, a la
 * consola de Linux, la paleta rgb del tema. */
enum { FT_TEXT, FT_BAR, FT_SEL, FT_HEAD, FT_DIM, FT_WARN, FT_MENU, FT_N };

void   ft_init(const Theme *t, int console_palette);
/* Com ft_init, però amb la paleta pal (la del tema amb els «color-…» del
 * phileasrc al damunt), com fa Phileas. pal = NULL: la del tema. */
void   ft_init_pal(const Theme *t, int console_palette, const short *pal);
void   ft_end(void);          /* restaura el terminal (també la paleta) */
void   ft_suspend(void);      /* abans d'executar un altre programa */
void   ft_resume(void);
attr_t ft_attr(int role);
void   ft_sync_size(void);    /* adopta la mida real del terminal */

#endif
