#ifndef COMU_MDSCAN_H
#define COMU_MDSCAN_H

#include <stddef.h>

/* Lectura dels elements d'Obsidian dins del cos d'una nota (sense la
 * capçalera YAML): enllaços, etiquetes i identificadors de bloc.
 *
 *   [[Nom]] [[Nom|text]] [[Nom#Secció]] [[Nom#^bloc]] [[Carpeta/Nom]] ![[Nom]]
 *   [text](Carpeta/Nom.md)            enllaç Markdown intern (no URL)
 *   #etiqueta #tema/subtema            no a l'inici d'una capçalera; cal una lletra
 *   … text del paràgraf ^bloc          identificador de bloc al final de la línia
 *
 * S'ignora el que hi ha dins de blocs de codi (``` i ~~~), codi en línia,
 * comentaris d'Obsidian (%% … %%) i comentaris HTML. */

typedef enum { CM_LINK, CM_TAG, CM_BLOCK } CmKind;

typedef struct {
    CmKind      kind;
    size_t      line;          /* número de línia (el primer és first_line) */
    const char *target;        /* enllaç: nom o camí, sense «.md»; etiqueta: sense «#»;
                                  bloc: sense «^» */
    size_t      target_len;
    const char *section;       /* enllaç: el que va darrere de «#» (sense «^»), o NULL */
    size_t      section_len;
    int         block;         /* enllaç: la secció és un ^bloc */
    const char *alias;         /* enllaç: el text darrere de «|», o NULL */
    size_t      alias_len;
    int         embed;         /* enllaç: ![[ ]] */
    const char *ctx;           /* la línia sencera on és */
    size_t      ctx_len;
} CmRef;

typedef void (*cm_ref_fn)(const CmRef *r, void *ud);

void cm_scan(const char *text, size_t len, size_t first_line, cm_ref_fn fn, void *ud);

/* Lectura dels enllaços [[ ]] d'un valor de propietat YAML (per a les
 * relacions amb tipus: `contradiu: "[[Font B]]"`). Cada CmRef porta line 0. */
void cm_scan_value(const char *value, size_t len, cm_ref_fn fn, void *ud);

/* 1 si el text (una etiqueta sense «#») és vàlid per a Obsidian: lletres,
 * xifres, _ - / i alguna cosa que no sigui una xifra. */
int cm_tag_valid(const char *s, size_t len);

#endif
