#ifndef PHILEAS_PALETTE_H
#define PHILEAS_PALETTE_H

/* Elements amb color configurable (temes: theme.c). Els de la barra, la
 * selecció i els menús, si no tenen cap color, van en vídeo invers (barra i
 * selecció) o amb els colors del text (menús). */
enum { PAL_H1, PAL_H2, PAL_H3, PAL_CODE, PAL_QUOTE, PAL_ERROR, PAL_MARK,
       PAL_TEXT, PAL_BG, PAL_BAR, PAL_BAR_BG, PAL_SEL, PAL_SEL_BG,
       PAL_MENU, PAL_MENU_BG, PAL_N };
#define PAL_DEFAULT (-1)   /* el color normal del terminal */

#endif
