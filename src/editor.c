#include "editor.h"
#include "utf8.h"
#include "util.h"
#include "wrap.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wctype.h>

void ed_init(Editor *e, int columns)
{
    memset(e, 0, sizeof *e);
    buf_init(&e->buf);
    e->columns = columns;
    e->width = columns;
    e->height = 1;
    e->want_x = -1;
}

void ed_free(Editor *e)
{
    buf_free(&e->buf);
    free(e->filename);
}

void ed_msg(Editor *e, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(e->msg, sizeof e->msg, fmt, ap);
    va_end(ap);
}

Line *ed_line(Editor *e, size_t y)
{
    Line *l = &e->buf.lines[y];

    wrap_line(l, e->width);
    return l;
}

size_t ed_cursor_row(Editor *e)
{
    return wrap_row_of(ed_line(e, e->cy), e->cx);
}

/* Amplada en pantalla de l->text[from..to) començant a la columna 0. */
static int span_width(const Line *l, size_t from, size_t to)
{
    int col = 0;

    while (from < to) {
        wchar_t wc;
        from += utf8_decode(l->text, l->len, from, &wc);
        col += utf8_width(wc, col);
    }
    return col;
}

int ed_cursor_x(Editor *e)
{
    Line *l = ed_line(e, e->cy);

    return span_width(l, l->rows[wrap_row_of(l, e->cx)], e->cx);
}

/* Posició de la fila r més propera a la columna x. En una fila que no és
 * l'última, el cursor no pot passar a l'inici de la fila següent. */
static size_t offset_at_x(const Line *l, size_t r, int x)
{
    size_t i = l->rows[r], end = wrap_row_end(l, r);
    int last = r + 1 == l->nrows;
    int col = 0;

    while (i < end) {
        wchar_t wc;
        size_t next;
        int w;

        utf8_decode(l->text, l->len, i, &wc);
        w = utf8_width(wc, col);
        if (col + w > x)
            break;
        next = utf8_next_cluster(l->text, l->len, i);
        if (!last && next >= end)
            break;
        col += w;
        i = next;
    }
    return i;
}

int ed_vpos_next(Editor *e, size_t *line, size_t *row)
{
    if (*row + 1 < ed_line(e, *line)->nrows) {
        ++*row;
        return 1;
    }
    if (*line + 1 < e->buf.nlines) {
        ++*line;
        *row = 0;
        return 1;
    }
    return 0;
}

int ed_vpos_prev(Editor *e, size_t *line, size_t *row)
{
    if (*row > 0) {
        --*row;
        return 1;
    }
    if (*line > 0) {
        --*line;
        *row = ed_line(e, *line)->nrows - 1;
        return 1;
    }
    return 0;
}

static int vpos_before(size_t l1, size_t r1, size_t l2, size_t r2)
{
    return l1 < l2 || (l1 == l2 && r1 < r2);
}

void ed_layout(Editor *e, int screen_cols, int screen_lines)
{
    size_t cr, tl, tr;

    e->width = e->columns < screen_cols ? e->columns : screen_cols;
    if (e->width < 1)
        e->width = 1;
    e->height = screen_lines - 2 > 0 ? screen_lines - 2 : 1;
    e->margin = 0;

    /* La fila de dalt pot haver quedat fora de rang si ha canviat l'amplada */
    if (e->top_line >= e->buf.nlines)
        e->top_line = e->buf.nlines - 1;
    if (e->top_row >= ed_line(e, e->top_line)->nrows)
        e->top_row = ed_line(e, e->top_line)->nrows - 1;

    cr = ed_cursor_row(e);
    if (vpos_before(e->cy, cr, e->top_line, e->top_row)) {
        e->top_line = e->cy;
        e->top_row = cr;
        return;
    }
    /* El cursor ha de quedar com a molt height-1 files per sota de dalt */
    tl = e->cy;
    tr = cr;
    for (int i = 0; i < e->height - 1 && ed_vpos_prev(e, &tl, &tr); i++)
        ;
    if (vpos_before(e->top_line, e->top_row, tl, tr)) {
        e->top_line = tl;
        e->top_row = tr;
    }
}

void ed_move_left(Editor *e)
{
    if (e->cx > 0) {
        e->cx = utf8_prev_cluster(e->buf.lines[e->cy].text, e->cx);
    } else if (e->cy > 0) {
        e->cy--;
        e->cx = e->buf.lines[e->cy].len;
    }
    e->want_x = -1;
}

void ed_move_right(Editor *e)
{
    Line *l = &e->buf.lines[e->cy];

    if (e->cx < l->len) {
        e->cx = utf8_next_cluster(l->text, l->len, e->cx);
    } else if (e->cy + 1 < e->buf.nlines) {
        e->cy++;
        e->cx = 0;
    }
    e->want_x = -1;
}

void ed_move_up(Editor *e)
{
    Line *l = ed_line(e, e->cy);
    size_t r = wrap_row_of(l, e->cx);

    if (e->want_x < 0)
        e->want_x = ed_cursor_x(e);
    if (r > 0) {
        e->cx = offset_at_x(l, r - 1, e->want_x);
    } else if (e->cy > 0) {
        l = ed_line(e, --e->cy);
        e->cx = offset_at_x(l, l->nrows - 1, e->want_x);
    } else {
        e->cx = 0;
    }
}

void ed_move_down(Editor *e)
{
    Line *l = ed_line(e, e->cy);
    size_t r = wrap_row_of(l, e->cx);

    if (e->want_x < 0)
        e->want_x = ed_cursor_x(e);
    if (r + 1 < l->nrows) {
        e->cx = offset_at_x(l, r + 1, e->want_x);
    } else if (e->cy + 1 < e->buf.nlines) {
        e->cx = offset_at_x(ed_line(e, ++e->cy), 0, e->want_x);
    } else {
        e->cx = l->len;
    }
}

void ed_row_start(Editor *e)
{
    Line *l = ed_line(e, e->cy);

    e->cx = l->rows[wrap_row_of(l, e->cx)];
    e->want_x = -1;
}

void ed_row_end(Editor *e)
{
    Line *l = ed_line(e, e->cy);
    size_t r = wrap_row_of(l, e->cx);

    if (r + 1 < l->nrows)
        e->cx = utf8_prev_cluster(l->text, l->rows[r + 1]);
    else
        e->cx = l->len;
    e->want_x = -1;
}

/* La pantalla i el cursor es desplacen junts, com a la majoria d'editors. */
void ed_page_up(Editor *e)
{
    for (int i = 0; i < (e->height > 1 ? e->height - 1 : 1); i++) {
        ed_vpos_prev(e, &e->top_line, &e->top_row);
        ed_move_up(e);
    }
}

void ed_page_down(Editor *e)
{
    for (int i = 0; i < (e->height > 1 ? e->height - 1 : 1); i++) {
        ed_vpos_next(e, &e->top_line, &e->top_row);
        ed_move_down(e);
    }
}

/* Lletres, xifres, el punt volat (l·l) i els apòstrofs (l'home) formen part
 * de la paraula. */
static int is_word_char(wchar_t wc)
{
    return wc != UTF8_INVALID &&
           (iswalnum((wint_t)wc) || wc == L'·' || wc == L'\'' || wc == L'’');
}

static int word_at(const Line *l, size_t i)
{
    wchar_t wc;

    utf8_decode(l->text, l->len, i, &wc);
    return is_word_char(wc);
}

void ed_word_next(Editor *e)
{
    Line *l = &e->buf.lines[e->cy];

    if (e->cx >= l->len) {
        if (e->cy + 1 < e->buf.nlines) {
            e->cy++;
            e->cx = 0;
        }
    } else {
        while (e->cx < l->len && word_at(l, e->cx))
            e->cx = utf8_next_cluster(l->text, l->len, e->cx);
        while (e->cx < l->len && !word_at(l, e->cx))
            e->cx = utf8_next_cluster(l->text, l->len, e->cx);
    }
    e->want_x = -1;
}

void ed_word_prev(Editor *e)
{
    Line *l = &e->buf.lines[e->cy];

    if (e->cx == 0) {
        if (e->cy > 0) {
            e->cy--;
            e->cx = e->buf.lines[e->cy].len;
        }
    } else {
        while (e->cx > 0 && !word_at(l, utf8_prev_cluster(l->text, e->cx)))
            e->cx = utf8_prev_cluster(l->text, e->cx);
        while (e->cx > 0 && word_at(l, utf8_prev_cluster(l->text, e->cx)))
            e->cx = utf8_prev_cluster(l->text, e->cx);
    }
    e->want_x = -1;
}

static int line_blank(const Line *l)
{
    for (size_t i = 0; i < l->len; i++)
        if (l->text[i] != ' ' && l->text[i] != '\t')
            return 0;
    return 1;
}

void ed_par_next(Editor *e)
{
    size_t y = e->cy + 1;

    while (y < e->buf.nlines && line_blank(&e->buf.lines[y]))
        y++;
    if (y < e->buf.nlines) {
        e->cy = y;
        e->cx = 0;
    } else {
        ed_doc_end(e);
    }
    e->want_x = -1;
}

void ed_par_prev(Editor *e)
{
    size_t y = e->cy;

    if (e->cx == 0) {
        while (y > 0 && line_blank(&e->buf.lines[--y]))
            ;
    }
    e->cy = y;
    e->cx = 0;
    e->want_x = -1;
}

void ed_doc_start(Editor *e)
{
    e->cy = e->cx = 0;
    e->want_x = -1;
}

void ed_doc_end(Editor *e)
{
    e->cy = e->buf.nlines - 1;
    e->cx = e->buf.lines[e->cy].len;
    e->want_x = -1;
}

void ed_goto_line(Editor *e, size_t n)
{
    if (n < 1)
        n = 1;
    if (n > e->buf.nlines)
        n = e->buf.nlines;
    e->cy = n - 1;
    e->cx = 0;
    e->want_x = -1;
}
