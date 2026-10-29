#ifndef PHILEAS_OUTLINE_H
#define PHILEAS_OUTLINE_H

#include "buffer.h"

#include <stddef.h>

/* Esquema del document: les capçaleres Markdown, amb "#" (1-6) o
 * subratllades amb === (1) o --- (2). Les de dins dels blocs de codi i de
 * les metadades no compten. */
typedef struct {
    size_t line;     /* línia de la capçalera al buffer */
    int    level;    /* 1-6 */
    char  *title;    /* text sense els marcadors (pot ser buit) */
    size_t words;    /* paraules de la secció, amb les subseccions i sense
                      * la capçalera mateixa */
} OutlineItem;

typedef struct {
    OutlineItem *items;
    size_t       n;
    size_t       first_words;   /* paraules abans de la primera capçalera */
} Outline;

/* Omple o (cal alliberar-lo amb outline_free). En text pla, buit. */
void   outline_build(Buffer *b, Outline *o);
void   outline_free(Outline *o);

/* Índex de la secció on és la línia (l'última capçalera que no la passa), o
 * OUTLINE_NONE si és abans de la primera. */
#define OUTLINE_NONE ((size_t)-1)
size_t outline_at(const Outline *o, size_t line);

#endif
