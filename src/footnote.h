#ifndef PHILEAS_FOOTNOTE_H
#define PHILEAS_FOOTNOTE_H

#include "buffer.h"

#include <stddef.h>

/* Notes al peu amb la sintaxi de pandoc: la crida [^etiqueta] dins del
 * text i la definició "[^etiqueta]: text" al principi d'un paràgraf, que
 * pot continuar en paràgrafs sagnats amb 4 espais. No compten les del
 * codi, les metadades ni l'HTML. Abans de cridar-les, buf_scan(). */

/* 1 si a la línia i hi pot haver notes (no és codi, metadades ni HTML). */
int    fn_line_ok(const Buffer *b, size_t i);

/* Número per a una nota nova: el més alt de les etiquetes numèriques + 1. */
unsigned long fn_next_number(const Buffer *b);

/* 1 si la línia i és dins d'una nota (la definició o un paràgraf sagnat
 * que la continua); deixa a *def la línia de la definició. */
int    fn_in_def(const Buffer *b, size_t i, size_t *def);

/* Crida de la línia pos.line que conté pos (del '[' fins just després del
 * ']'); si no n'hi ha cap, la més propera de la mateixa línia. Retorna 1 i
 * deixa a *a, *z el tram de la crida sencera. */
int    fn_ref_near(const Buffer *b, Pos pos, size_t *a, size_t *z);

/* Línia de la definició de l'etiqueta label (n bytes). */
int    fn_find_def(const Buffer *b, const char *label, size_t n, size_t *line);

/* Primera crida de l'etiqueta: deixa a *after la posició just després. */
int    fn_find_ref(const Buffer *b, const char *label, size_t n, Pos *after);

/* Insereix la crida [^n] a at i la definició buida al final del document,
 * en el mateix pas de desfer. Retorna on s'escriu el text de la nota. */
Pos    fn_insert(Buffer *b, Pos at, unsigned long n);

/* Renumera les etiquetes numèriques en l'ordre de la primera crida (les
 * definicions sense crida van al final) i ordena les definicions en aquest
 * ordre, als mateixos llocs on eren. Les etiquetes amb nom es queden. Deixa
 * a *notes el nombre de notes. Retorna 1 si ha canviat alguna cosa. */
int    fn_renumber(Buffer *b, size_t *notes);

#endif
