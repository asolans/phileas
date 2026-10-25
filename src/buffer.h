#ifndef PHILEAS_BUFFER_H
#define PHILEAS_BUFFER_H

#include <stddef.h>

/* Una línia lògica: en Markdown, un paràgraf sencer. */
typedef struct {
    char   *text;      /* UTF-8, acabat en '\0', sense salt de línia */
    size_t  len, cap;

    /* Cache del wordwrap (wrap.c): inici en bytes de cada fila visual */
    size_t *rows;
    size_t  nrows, rows_cap;
    int     wrap_w;    /* amplada amb què s'ha calculat; 0 = per calcular */
    int     hang;      /* sagnat penjant de les files 2a i següents (llistes, cites) */

    /* Cache de l'estimació de pàgines: mitges línies impreses; 0 = per calcular */
    size_t  print_lines;
} Line;

typedef struct {
    Line   *lines;
    size_t  nlines, cap;   /* sempre hi ha com a mínim una línia */

    /* Format del fitxer original, per desar-lo igual */
    int     crlf;          /* salts de línia "\r\n" */
    int     bom;           /* marca d'ordre de bytes UTF-8 al principi */
    int     final_newline; /* el fitxer acaba amb salt de línia */

    int     modified;
} Buffer;

/* Posició al text: línia i byte dins la línia (sempre a l'inici d'un
 * caràcter UTF-8). */
typedef struct {
    size_t line, col;
} Pos;

void buf_init(Buffer *b);
void buf_free(Buffer *b);

/* Insereix s (pot contenir '\n') a at. Retorna la posició just després. */
Pos  buf_insert(Buffer *b, Pos at, const char *s, size_t n);

/* Esborra el text entre a i z (a anterior a z). */
void buf_delete(Buffer *b, Pos a, Pos z);

/* Còpia del text entre a i z, amb '\n' entre línies (cal alliberar-la). */
char *buf_extract(const Buffer *b, Pos a, Pos z, size_t *n);

/* Retornen 0 si va bé, o -1 amb errno. Si buf_load falla, b no canvia. */
int  buf_load(Buffer *b, const char *path);
int  buf_save(Buffer *b, const char *path);

/* Estimació orientativa de pàgines si s'imprimís en A4 a 12 punts. */
size_t buf_pages(Buffer *b);

#endif
