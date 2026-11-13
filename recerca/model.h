#ifndef RECERCA_MODEL_H
#define RECERCA_MODEL_H

#include <stddef.h>

#include "sqlite3.h"

/* El que mostra la pantalla, carregat de l'índex a la memòria: totes les
 * notes (sense el text) i les etiquetes. Amb 2.000 notes són uns 200 KB i
 * es carreguen en menys de 0,1 s a la Pi. */

typedef struct {
    long long id;
    char     *cami, *carpeta, *nom, *fold;   /* fold: nom en minúscules i sense accents */
    char     *tipus, *data;                  /* poden ser "" */
    long long paraules, mtime;
} MNote;

typedef struct {
    long long fitxa;
    char     *tag;                           /* en minúscules, sense «#» */
} MTag;

typedef enum { NAV_CARPETES, NAV_ETIQUETES, NAV_TIPUS, NAV_N } NavMode;

/* Una entrada del panell de navegació. key = NULL vol dir «totes les
 * fitxes»; per a les carpetes, "" és l'arrel del vault. */
typedef struct {
    char  *label;
    char  *key;
    int    depth;
    size_t count;
} MNav;

typedef enum { SORT_NOM, SORT_DATA, SORT_MODIF, SORT_PARAULES, SORT_N } SortMode;

typedef struct {
    MNote   *notes;
    size_t   n;
    MTag    *tags;      /* ordenades per fitxa */
    size_t   ntags;
    MNav    *nav;
    size_t   nnav;
    NavMode  mode;
} Model;

int  model_load(Model *m, sqlite3 *db);      /* 0 si va bé; conserva el mode */
void model_free(Model *m);
void model_set_mode(Model *m, NavMode mode); /* refà les entrades de navegació */

/* Notes de l'entrada nav (NULL = totes) que contenen filter (ja plegat amb
 * cm_fold; "" = cap filtre), ordenades. *out: índexs a m->notes (malloc). */
size_t model_list(const Model *m, const MNav *nav, const char *filter, SortMode sort, size_t **out);

long        model_find(const Model *m, long long id);   /* índex a m->notes, o -1 */
const char *model_sort_label(SortMode s);
const char *model_mode_label(NavMode mode);

/* Lletra del tipus per a la llista: F fons, N nota de camp, B bibliografia… */
char model_type_letter(const char *tipus);

#endif
