#ifndef FABULA_COMPILE_H
#define FABULA_COMPILE_H

#include "project.h"

#include <stdio.h>

/* Compilar: el manuscrit sencer en un sol document Markdown, en l'ordre dels
 * capítols. Només es llegeix; el projecte no es toca. */
typedef struct {
    int promote;      /* puja un nivell els títols: ## → #, ### → ## (els capítols passen a nivell 1) */
    int scene_sep;    /* les escenes (###) es converteixen en un separador «* * *» */
    int keep_notes;   /* conserva les notes %%així%% (per defecte es treuen) */
    int keep_links;   /* conserva els [[enllaços]] (per defecte es queda el text: [[Anna|l'Anna]] → l'Anna) */
} CompileOpts;

typedef struct {
    size_t chapters;
    size_t words;       /* paraules de prosa del manuscrit */
    size_t pending;     /* TODO i [?] que queden al text */
} CompileStats;

/* Escriu el document a out: un bloc YAML amb el títol, l'autor i l'idioma del
 * projecte i, després, cada capítol sense la seva capçalera YAML ni els
 * comentaris de metadades de sota els títols. Retorna 0, o -1 amb err. */
int project_compile(const Project *p, const CompileOpts *o, FILE *out, CompileStats *st,
                    char *err, size_t esz);

/* Converteix un .md amb pandoc (docx, odt, epub, pdf, html…). Retorna 0, o -1
 * amb err (p. ex. si pandoc no és instal·lat). */
int compile_pandoc(const char *md, const char *out, char *err, size_t esz);

/* Compila a un fitxer: .md (escriptura atòmica) o, per a qualsevol altra
 * extensió, a través de pandoc (cal que hi sigui). No sobreescriu res si falla. */
int project_compile_file(const Project *p, const CompileOpts *o, const char *outpath, CompileStats *st,
                         char *err, size_t esz);

/* La sortida demana pandoc? (qualsevol extensió que no sigui .md ni .markdown) */
int compile_needs_pandoc(const char *out);

#endif
