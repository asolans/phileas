#ifndef PHILEAS_CALAIX_H
#define PHILEAS_CALAIX_H

#include <stddef.h>

/* Calaix d'idees: un fitxer Markdown orientat a la referència. Cada entrada és
 *
 *     ## font · p. pàgina · #etiquetes
 *
 *     text de la idea
 *
 *     ∙ AAAA-MM-DD · /camí/del/document.md:línia
 *
 * La capçalera només porta les parts que hi ha: sense font ni pàgina és
 * «## #etiquetes». El peu («∙ data · origen») tanca l'entrada, de manera que
 * el text pot contenir línies «## …». Les entrades antigues («## AAAA-MM-DD
 * HH:MM · origen · #etiquetes», sense peu) es continuen llegint.
 * Es desa al camí de l'opció «calaix» del phileasrc. */

#define CAL_OLD    1   /* format antic: sense peu */
#define CAL_CLOSED 2   /* acaba en un peu */

typedef struct {
    char   *text;   /* tot el fitxer */
    size_t  len;
    size_t *head;   /* inici de la capçalera de cada entrada */
    size_t *end;    /* fi de l'entrada (després del peu, o abans de la capçalera següent) */
    unsigned char *flags;   /* CAL_* */
    size_t  n;
} Calaix;

/* Carrega el fitxer. Un fitxer que no existeix és un calaix buit (0).
 * Retorna 0, o -1 amb errno si no es pot llegir. */
int  calaix_load(const char *path, Calaix *c);
void calaix_free(Calaix *c);

/* La capçalera de l'entrada i, a len, la seva longitud. */
const char *calaix_head(const Calaix *c, size_t i, size_t *len);

/* Línia (des d'1) on comença la capçalera de l'entrada. */
size_t calaix_line(const Calaix *c, size_t i);

/* 1 si el text de l'entrada conté filter (sense distingir majúscules). */
int  calaix_match(const Calaix *c, size_t i, const char *filter);

/* Afegeix una entrada al final del fitxer, amb escriptura atòmica.
 * font, page i tags (poden ser NULL o buits) formen la capçalera; tags són les
 * etiquetes escrites per l'usuari. origin és el document i la línia
 * («/data/textos/a.md:4»), per al peu. Retorna 0, o -1 amb el motiu a err. */
int  calaix_append(const char *path, const char *font, const char *page,
                   const char *tags, const char *origin,
                   const char *body, size_t n, char *err, size_t errsz);

/* Estat persistent: la font activa (per defecte de cada idea nova, fins que
 * es canvia), l'última pàgina d'aquesta font i les fonts recents. Es desa al
 * fitxer «calaix-font» del directori de configuració. */
#define CALAIX_RECENT 20
#define CALAIX_FONT_MAX 160
#define CALAIX_PAGE_MAX 32

typedef struct {
    char   font[CALAIX_FONT_MAX];   /* buit = cap font */
    char   page[CALAIX_PAGE_MAX];
    char   recent[CALAIX_RECENT][CALAIX_FONT_MAX];   /* la més recent primer */
    size_t nrecent;
} CalaixState;

void calaix_state_load(CalaixState *s);          /* sense fitxer, buit */
int  calaix_state_save(const CalaixState *s);    /* atòmic; -1 amb errno */
/* Canvia la font activa (buida = cap). Si canvia, s'esborra la pàgina; una
 * font no buida passa al capdavant dels recents. */
void calaix_state_set_font(CalaixState *s, const char *font);

#endif
