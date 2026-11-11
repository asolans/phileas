#ifndef FABULA_TABLE_H
#define FABULA_TABLE_H

#include "project.h"

/* L'outliner: el manuscrit en forma de taula (capítols i escenes) amb una
 * columna per metadada. Sense ordre, és jeràrquic (cada capítol amb les seves
 * escenes); amb una columna d'ordre, és pla (només escenes, i els capítols que
 * no en tenen) i ordenat. El filtre mira el títol, la sinopsi i totes les
 * metadades. */
enum { TC_TITLE, TC_POV, TC_LLOC, TC_DATA, TC_ESTAT, TC_WORDS, TC_N };
#define TC_NONE (-1)

typedef struct {
    FNode *node;
    int    depth;     /* 0 capítol, 1 escena (en una taula plana, sempre 0) */
    size_t seq;       /* ordre original (per a un ordre estable) */
} TRow;

typedef struct {
    TRow  *rows;
    size_t n, cap;
    int    flat;      /* ordenada per una columna */
} Table;

/* sort_col: TC_NONE o la columna; desc inverteix l'ordre (els buits van sempre al final) */
void table_build(Table *t, const Project *p, const char *filter, int sort_col, int desc);
void table_free(Table *t);
long table_find(const Table *t, const FNode *n);

/* Text d'una cel·la. En una taula plana, el títol d'una escena porta el del
 * capítol («Capítol › Escena»). Retorna buf. */
const char *table_cell(const Table *t, const FNode *n, int col, char *buf, size_t bsz);

/* Nom de la columna i la clau de metadada que edita (NULL si no s'edita així) */
const char *table_col_name(int col);
const char *table_col_key(int col);

#endif
