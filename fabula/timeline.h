#ifndef FABULA_TIMELINE_H
#define FABULA_TIMELINE_H

#include "project.h"

/* Línia temporal: les escenes del manuscrit (o el capítol, si no en té) per
 * data, no per ordre del text. La data és la metadada `data` (AAAA, AAAA-MM o
 * AAAA-MM-DD, heretada del capítol). */

typedef struct {
    FNode *node;        /* escena, o capítol sense escenes */
    size_t seq;         /* posició en l'ordre del text (0 = la primera) */
    long   lo, hi;      /* interval de dies (des de 1970; pot ser negatiu) */
    int    dated;       /* té data (i s'entén) */
    int    back;        /* el text hi arriba després d'una escena posterior: salt enrere */
    int    bad;         /* data que no s'entén (és a la llista, sense data) */
} TLItem;

typedef struct {
    TLItem *items;      /* per ordre cronològic; primer les datades, després les sense data */
    size_t  n;
    size_t  dated;      /* quantes tenen data */
    size_t  backs;      /* salts enrere */
    size_t  undated;
    size_t  bad;
} Timeline;

int  timeline_build(const Project *p, Timeline *t);   /* 0, o -1 si no hi ha memòria */
void timeline_free(Timeline *t);

/* Una data parcial a un interval de dies. 0 si és bona, -1 si no s'entén. */
int  timeline_range(const char *s, long *lo, long *hi);

#endif
