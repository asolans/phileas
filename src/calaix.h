#ifndef PHILEAS_CALAIX_H
#define PHILEAS_CALAIX_H

#include <stddef.h>

/* Calaix d'idees: un fitxer Markdown on cada entrada comença per una
 * capçalera «## AAAA-MM-DD HH:MM · origen · #etiquetes», seguida del text.
 * Es desa al camí de l'opció «calaix» del phileasrc. */

typedef struct {
    char   *text;   /* tot el fitxer */
    size_t  len;
    size_t *head;   /* inici de la capçalera de cada entrada */
    size_t *end;    /* fi de l'entrada (abans de la següent capçalera) */
    size_t  n;
} Calaix;

/* Carrega el fitxer. Un fitxer que no existeix és un calaix buit (0).
 * Retorna 0, o -1 amb errno si no es pot llegir. */
int  calaix_load(const char *path, Calaix *c);
void calaix_free(Calaix *c);

/* La capçalera de l'entrada i, a len, la seva longitud. */
const char *calaix_head(const Calaix *c, size_t i, size_t *len);

/* 1 si el text de l'entrada conté filter (sense distingir majúscules). */
int  calaix_match(const Calaix *c, size_t i, const char *filter);

/* Afegeix una entrada al final del fitxer, amb escriptura atòmica.
 * origen és el text de la capçalera («capitol.md, línia 4»); tags són les
 * etiquetes escrites per l'usuari (poden ser NULL). Retorna 0, o -1 amb el
 * motiu a err. */
int  calaix_append(const char *path, const char *origin, const char *tags,
                   const char *body, size_t n, char *err, size_t errsz);

#endif
