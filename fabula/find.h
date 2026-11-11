#ifndef FABULA_FIND_H
#define FABULA_FIND_H

#include "project.h"

/* Cerca i substitució de text a tot el projecte (capítols i fitxes). La cerca
 * es fa sobre el text del fitxer, línia a línia; la substitució és literal i
 * només toca els bytes de cada coincidència. */
enum {
    FF_ICASE = 1,   /* sense majúscules ni accents */
    FF_WORD  = 2,   /* només paraules senceres */
    FF_META  = 4,   /* també dins de les metadades (capçalera YAML i comentaris sota els títols) */
};

typedef struct {
    FNode  *file;
    size_t  start, end;   /* bytes de la coincidència al fitxer */
    size_t  line;         /* línia del buffer (paràgraf) de Phileas, 0 = la primera */
    size_t  scene;        /* escena (1 = la primera) o 0 */
    char   *context;      /* fragment amb la coincidència entre « i » */
} FHit;

typedef struct {
    FHit   *hits;
    size_t  n;
    size_t  files;        /* fitxers amb alguna coincidència */
} FFind;

/* Cerca needle (sense salts de línia) a tot el projecte. Retorna 0 o -1 amb err. */
int  find_in_project(const Project *p, const char *needle, unsigned opts, FFind *out,
                     char *err, size_t esz);
void find_free(FFind *f);

/* Substitueix per repl les coincidències on selected[i] != 0 (o totes, si
 * selected és NULL). Abans de tocar un fitxer en fa una instantània; si un
 * fitxer ha canviat des de la cerca, no el toca i ho diu. *files i *count
 * (opcionals) diuen què s'ha canviat. */
int  find_replace(Project *p, const FFind *f, const unsigned char *selected, const char *repl,
                  time_t now, size_t *files, size_t *count, char *err, size_t esz);

#endif
