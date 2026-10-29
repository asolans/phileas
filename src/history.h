#ifndef PHILEAS_HISTORY_H
#define PHILEAS_HISTORY_H

#include "editor.h"

#include <stddef.h>

/* Historial de documents (C2 i C3): el fitxer «historial» del directori de
 * configuració, amb una línia per document, el més recent primer:
 *     LÍNIA BYTE CAMÍ-ABSOLUT
 * Serveix per tornar el cursor on era i per a la llista de recents. */

#define HISTORY_MAX 100

typedef struct {
    char  *path;
    size_t line, col;
} HistEntry;

typedef struct {
    HistEntry *items;
    size_t     n;
} History;

void history_path(char *out, size_t size);
void history_load(History *h);           /* sense fitxer, buit */
int  history_save(const History *h);     /* atòmic; -1 amb errno */
void history_free(History *h);

/* Índex de l'entrada del camí absolut, o -1 */
long history_find(const History *h, const char *abspath);
/* Posa (o mou) el camí al principi amb aquesta posició */
void history_touch(History *h, const char *abspath, size_t line, size_t col);
void history_remove(History *h, size_t i);
/* Treu els camins que ja no són fitxers. Retorna quants n'ha tret. */
size_t history_prune_missing(History *h);

/* Anota el document obert i la posició del cursor, si té nom i existeix
 * al disc. Retorna -1 si no s'ha pogut desar l'historial. */
int  history_remember(const Editor *e);
/* Després d'ed_open: si el document és a l'historial i l'opció
 * recordar-posicio és activa, hi torna el cursor. Retorna 1 si ho ha fet. */
int  history_restore(Editor *e);

#endif
