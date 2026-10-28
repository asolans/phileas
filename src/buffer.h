#ifndef PHILEAS_BUFFER_H
#define PHILEAS_BUFFER_H

#include "md.h"
#include "undo.h"

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
    /* Cache del recompte de paraules: paraules + 1; 0 = per calcular */
    size_t  words;

    /* Tipus de bloc Markdown (MD_*); -1 = per calcular. st és l'estat
     * després d'aquesta línia. Els manté al dia buf_scan(). */
    int     kind;
    MdState st;
} Line;

typedef struct {
    Line   *lines;
    size_t  nlines, cap;   /* sempre hi ha com a mínim una línia */

    /* Format del fitxer original, per desar-lo igual */
    int     crlf;          /* salts de línia "\r\n" */
    int     bom;           /* marca d'ordre de bytes UTF-8 al principi */
    int     final_newline; /* el fitxer acaba amb salt de línia */

    int     modified;

    /* Text pla (fitxers que no són Markdown): cada línia del fitxer és una
     * línia del buffer, sense ajuntar paràgrafs ni separacions */
    int     plain;

    /* Interval de línies modificades des de l'últim buf_scan() */
    size_t  scan_from, scan_to;

    Undo    undo;
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

/* Desfer/refer: tota modificació feta amb buf_insert i buf_delete queda a
 * l'historial. buf_undo_mark separa grups (una ordre, una paraula escrita).
 * buf_undo i buf_redo retornen 0 si no hi havia res i deixen a *cursor la
 * posició on ha tingut lloc el canvi. */
void buf_undo_mark(Buffer *b);
int  buf_undo(Buffer *b, Pos *cursor);
int  buf_redo(Buffer *b, Pos *cursor);

/* Còpia del text entre a i z, amb '\n' entre línies (cal alliberar-la). */
char *buf_extract(const Buffer *b, Pos a, Pos z, size_t *n);

/* Adapta un text enganxat des de fora per inserir-lo a la línia line: com en
 * carregar un fitxer, cada línia en blanc entre blocs és una separació de
 * paràgraf (no una línia buida), excepte dins de blocs de codi. No ajunta
 * línies. Retorna una còpia nova (cal alliberar-la). */
char *buf_paste_text(Buffer *b, size_t line, const char *s, size_t n, size_t *out_n);

/* Actualitza el tipus de bloc de les línies modificades. */
void buf_scan(Buffer *b);

/* Marca tot el buffer per tornar-lo a classificar. */
void buf_rescan_all(Buffer *b);

/* 1 si després de la línia i hi ha una separació de paràgraf: al fitxer
 * és una línia en blanc i a la pantalla, una fila buida que no existeix al
 * buffer. */
int  buf_gap_after(Buffer *b, size_t i);

/* 1 si el nom de fitxer és de Markdown (.md, .markdown, .mdown, .mkd,
 * .txt): els altres s'obren com a text pla. */
int  buf_is_markdown_path(const char *path);

/* Retornen 0 si va bé, o -1 amb errno. Si buf_load falla, b no canvia.
 * El mode (b->plain) s'ha de triar abans de carregar.
 * En carregar, cada línia en blanc entre blocs es converteix en una
 * separació de paràgraf, i les línies d'un paràgraf tallat a mà s'ajunten.
 * En desar, cada separació torna a ser una línia en blanc. */
int  buf_load(Buffer *b, const char *path);
int  buf_save(Buffer *b, const char *path);

/* Estimació orientativa de pàgines si s'imprimís en A4 a 12 punts. */
size_t buf_pages(Buffer *b);

/* Paraules del document: grups de caràcters sense espais amb alguna lletra
 * o xifra (els marcadors de Markdown sols, com «-» o «#», no compten). */
size_t buf_words(Buffer *b);

#endif
