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

/* Línia on acaba la secció k (exclusiva): la de la capçalera següent del
 * mateix nivell o superior, o nlines. */
size_t outline_end(const Outline *o, size_t k, size_t nlines);

/* Per moure la secció k per sobre de l'anterior (dir < 0) o per sota de la
 * següent del mateix nivell, dins de la mateixa secció pare: deixa a a, b,
 * c els blocs [a, b) i [b, c) que s'han d'intercanviar, i a *head la línia
 * on quedarà la capçalera. Retorna 0 si no n'hi ha cap (és la primera o
 * l'última del seu nivell). */
int    outline_move(const Outline *o, size_t k, int dir, size_t nlines,
                    size_t *a, size_t *b, size_t *c, size_t *head);

#endif
