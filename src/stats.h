#ifndef PHILEAS_STATS_H
#define PHILEAS_STATS_H

#include <stddef.h>

/* Estadístiques d'un text (UTF-8; '\n' separa els paràgrafs) */
typedef struct {
    size_t words;        /* grups amb alguna lletra o xifra */
    size_t chars;        /* caràcters, amb espais (sense els salts de línia) */
    size_t chars_nospace;
    size_t sentences;
    size_t paragraphs;   /* paràgrafs no buits */
    double pages;        /* pàgines estàndard de 1.800 caràcters amb espais */
    double minutes;      /* temps de lectura, a STATS_WPM paraules per minut */
} DocStats;

#define STATS_PAGE_CHARS 1800
#define STATS_WPM        200

void doc_stats(const char *text, size_t len, DocStats *s);

#endif
