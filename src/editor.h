#ifndef PHILEAS_EDITOR_H
#define PHILEAS_EDITOR_H

#include "buffer.h"

enum {
    SEL_NONE,   /* sense selecció */
    SEL_SHIFT,  /* Maj+fletxes: un moviment sense Maj la cancel·la */
    SEL_MARK,   /* Ctrl+Espai: els moviments l'amplien fins que es cancel·la */
};

typedef struct {
    Buffer  buf;
    char   *filename;          /* NULL si el document encara no té nom */

    size_t  cy, cx;            /* cursor: línia i byte dins la línia */
    int     want_x;            /* columna visual desitjada per a ↑/↓; -1 = la del cursor */
    size_t  top_line, top_row; /* primera fila visual a dalt de la pantalla */

    int     sel_mode;
    Pos     anchor;            /* extrem fix de la selecció */

    char   *clip;              /* porta-retalls intern */
    size_t  clip_len;

    int     columns;           /* amplada de text configurada */
    int     center;            /* columna de text centrada a la pantalla */
    int     width, height;     /* àrea de text real */
    int     margin;            /* columna de pantalla on comença el text */

    char    msg[256];          /* missatge de la línia inferior */
    int     quit;
    int     quit_confirm;      /* s'ha avisat de canvis sense desar */
} Editor;

void  ed_init(Editor *e, int columns);
void  ed_free(Editor *e);
void  ed_msg(Editor *e, const char *fmt, ...);

/* Adapta l'àrea de text a la mida del terminal i fa visible el cursor. */
void  ed_layout(Editor *e, int screen_cols, int screen_lines);

Line *ed_line(Editor *e, size_t y);      /* línia amb el wordwrap calculat */
size_t ed_cursor_row(Editor *e);
int   ed_cursor_x(Editor *e);            /* columna dins l'àrea de text */

/* Avancen o recules una fila visual; retornen 0 als extrems del document. */
int   ed_vpos_next(Editor *e, size_t *line, size_t *row);
int   ed_vpos_prev(Editor *e, size_t *line, size_t *row);

/* Moviment */
void  ed_move_left(Editor *e);
void  ed_move_right(Editor *e);
void  ed_move_up(Editor *e);
void  ed_move_down(Editor *e);
void  ed_row_start(Editor *e);
void  ed_row_end(Editor *e);
void  ed_page_up(Editor *e);
void  ed_page_down(Editor *e);
void  ed_word_prev(Editor *e);
void  ed_word_next(Editor *e);
void  ed_par_prev(Editor *e);
void  ed_par_next(Editor *e);
void  ed_doc_start(Editor *e);
void  ed_doc_end(Editor *e);
void  ed_goto_line(Editor *e, size_t n);   /* n comença a 1 */

/* Selecció */
int   ed_selection(Editor *e, Pos *a, Pos *z);   /* 1 si n'hi ha i no és buida */
void  ed_sel_start(Editor *e, int mode);
void  ed_sel_clear(Editor *e);
void  ed_select_all(Editor *e);

/* Edició: si hi ha selecció, primer s'esborra (la substitueix) */
void  ed_insert(Editor *e, const char *s, size_t n);
void  ed_newline(Editor *e);     /* continua llistes i cites Markdown */
void  ed_backspace(Editor *e);
void  ed_delete(Editor *e);
void  ed_delete_word_prev(Editor *e);
void  ed_delete_word_next(Editor *e);

/* Porta-retalls: retornen el nombre de caràcters, o 0 si no hi havia res */
size_t ed_copy(Editor *e);
size_t ed_cut(Editor *e);
size_t ed_paste(Editor *e);

#endif
