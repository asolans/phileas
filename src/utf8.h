#ifndef PHILEAS_UTF8_H
#define PHILEAS_UTF8_H

#include <stddef.h>
#include <wchar.h>

#define TAB_WIDTH    4
#define UTF8_INVALID ((wchar_t)-1)

/* Descodifica el caràcter que comença a s[i] (i < len). Retorna la seva
 * longitud en bytes. Si la seqüència no és UTF-8 vàlid, *wc = UTF8_INVALID
 * i retorna 1: el byte es conserva tal qual i es mostra com a '?'. */
size_t utf8_decode(const char *s, size_t len, size_t i, wchar_t *wc);

/* Codifica wc a out (fins a 4 bytes). Retorna la longitud. */
size_t utf8_encode(wchar_t wc, char out[4]);

size_t utf8_next(const char *s, size_t len, size_t i);
size_t utf8_prev(const char *s, size_t i);

/* Com next/prev, però sense separar un caràcter dels seus diacrítics
 * combinats (amplada 0). És la unitat de moviment del cursor. */
size_t utf8_next_cluster(const char *s, size_t len, size_t i);
size_t utf8_prev_cluster(const char *s, size_t i);

/* Nombre de caràcters a s[0..end). */
size_t utf8_count(const char *s, size_t end);

/* 0 per a caràcters de control, bytes invàlids i punts de codi sense
 * amplada definida: es mostren com a '?'. */
int utf8_printable(wchar_t wc);

/* Amplada en pantalla de wc si comença a la columna col. */
int utf8_width(wchar_t wc, int col);

#endif
