#ifndef PHILEAS_THEME_H
#define PHILEAS_THEME_H

#include "editor.h"

#include <stddef.h>

/* Un tema dona un color a cada element de e->palette. A la consola de
 * Linux, si en té, també redefineix la paleta de 16 colors de la consola
 * (rgb), de manera que «negre» o «blanc» poden ser qualsevol color. */
typedef struct {
    const char   *name;     /* per al phileasrc i la paleta */
    const char   *label;    /* per als menús i els missatges */
    short         pal[PAL_N];
    const unsigned *rgb;    /* 16 colors 0xRRGGBB, o NULL */
} Theme;

size_t       theme_count(void);
const Theme *theme_at(size_t i);
const Theme *theme_find(const char *name);   /* NULL si no existeix */

/* Copia els colors del tema a e->palette i el deixa com a tema actual. */
void         theme_apply(Editor *e, const Theme *t);

/* Color pel nom: 0-15, PAL_DEFAULT per a «cap» o -2 si no existeix; i el
 * nom d'un color. */
const char  *theme_color_name(short c);
short        theme_color_parse(const char *name);

#endif
