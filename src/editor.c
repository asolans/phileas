#include "editor.h"
#include "md.h"
#include "spell.h"
#include "theme.h"
#include "utf8.h"
#include "util.h"
#include "wrap.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wctype.h>

/* Files en blanc per sobre d'un document buit en obrir-lo */
#define NEW_DOC_BLANK 2

void ed_init(Editor *e, int columns)
{
    char code[16];

    memset(e, 0, sizeof *e);
    buf_init(&e->buf);
    e->columns = columns;
    e->width = columns;
    e->height = 1;
    e->want_x = -1;
    e->autosave = e->autosave_last = 30;
    e->autosave_max = 300;
    e->top_blank = NEW_DOC_BLANK;
    e->margin_left = e->margin_right = 4;
    e->colors = 1;
    e->tw_mode = TW_SALT;
    e->tw_limit = 80;
    e->tw_anchor = 50;
    snprintf(e->calaix, sizeof e->calaix, "%s/calaix.md", getenv("HOME") ? getenv("HOME") : ".");
    snprintf(e->calaix_pi, sizeof e->calaix_pi, "%s/BYOK/calaix.md", getenv("HOME") ? getenv("HOME") : ".");
    snprintf(e->obs_inbox, sizeof e->obs_inbox, "Inbox-byok");
    snprintf(e->obs_tipus, sizeof e->obs_tipus, "tipus");
    theme_apply(e, theme_at(0));
    e->console_palette = 1;
    e->remember_pos = 1;
    /* Rotació de F8: català i castellà, començant pel de la configuració
     * regional si és un d'aquests dos */
    spell_code(NULL, code, sizeof code);
    snprintf(e->langs[0], sizeof e->langs[0], "%s", strcmp(code, "es") ? "ca" : "es");
    snprintf(e->langs[1], sizeof e->langs[1], "%s", strcmp(code, "es") ? "es" : "ca");
    e->nlangs = 2;
}

void ed_free(Editor *e)
{
    buf_free(&e->buf);
    free(e->filename);
    free(e->clip);
}

int ed_open(Editor *e, const char *path)
{
    int plain = path && !buf_is_markdown_path(path), old = e->buf.plain;

    e->buf.plain = plain;
    if (!path || buf_load(&e->buf, path) < 0) {
        if (path && errno != ENOENT) {
            e->buf.plain = old;
            return -1;
        }
        buf_free(&e->buf);
        buf_init(&e->buf);
        e->buf.plain = plain;
        if (path)
            ed_msg(e, "Fitxer nou: %s", path);
    }
    free(e->filename);
    e->filename = path ? xstrdup(path) : NULL;
    e->cy = e->cx = 0;
    e->top_line = e->top_row = 0;
    /* El final del text a l'ancoratge; un document buit comença a dalt, amb
     * dues files en blanc, i en mode salt la primera vegada que passa el
     * límit puja a l'ancoratge (en clàssic, el text puja fila a fila) */
    e->recenter = e->buf.nlines > 1 || e->buf.lines[0].len > 0;
    e->top_blank = e->recenter ? 0 : NEW_DOC_BLANK;
    e->want_x = -1;
    e->sel_mode = SEL_NONE;
    e->last_act = ACT_NONE;
    e->dirty_since = 0;
    e->autosave_warned = 0;
    e->quit_confirm = 0;
    /* Normalment s'obre un document per continuar-lo: cursor al final */
    ed_doc_end(e);
    return 0;
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

    size_t r = wrap_row_of(l, e->cx);

    return wrap_row_indent(l, r) + span_width(l, l->rows[r], e->cx);
}

/* Posició de la fila r més propera a la columna x. En una fila que no és
 * l'última, el cursor no pot passar a l'inici de la fila següent. */
static size_t offset_at_x(const Line *l, size_t r, int x)
{
    size_t i = l->rows[r], end = wrap_row_end(l, r);
    int last = r + 1 == l->nrows;
    int col = 0;

    x -= wrap_row_indent(l, r);

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

/* Files de pantalla de la línia y: les del wordwrap més la fila buida de
 * separació de paràgraf, si n'hi ha. */
static size_t screen_rows(Editor *e, size_t y)
{
    return ed_line(e, y)->nrows + (size_t)buf_gap_after(&e->buf, y);
}

int ed_vpos_next(Editor *e, size_t *line, size_t *row)
{
    if (*row + 1 < screen_rows(e, *line)) {
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
        *row = screen_rows(e, *line) - 1;
        return 1;
    }
    return 0;
}

static int vpos_before(size_t l1, size_t r1, size_t l2, size_t r2)
{
    return l1 < l2 || (l1 == l2 && r1 < r2);
}

/* Posa la fila de dalt de manera que el cursor quedi row files per sota.
 * Si abans s'arriba a l'inici del document, la resta són files en blanc
 * per sobre, com un full que comença a mitja pantalla. */
static void put_cursor_at(Editor *e, int row)
{
    size_t tl = e->cy, tr = ed_cursor_row(e);
    int i;

    for (i = 0; i < row && ed_vpos_prev(e, &tl, &tr); i++)
        ;
    e->top_line = tl;
    e->top_row = tr;
    e->top_blank = row - i;
}

/* Fila de pantalla del cursor (des de la fila de dalt), o -1 si és per
 * sobre; si és més avall que max, retorna max + 1. */
static int cursor_screen_row(Editor *e, int max)
{
    size_t tl = e->top_line, tr = e->top_row, cr = ed_cursor_row(e);
    int row = e->top_blank;

    if (vpos_before(e->cy, cr, tl, tr))
        return -1;
    while (tl != e->cy || tr != cr) {
        if (row > max || !ed_vpos_next(e, &tl, &tr))
            return max + 1;
        row++;
    }
    return row;
}

/* Desplaçament de màquina d'escriure: el text no arriba a baix de la
 * pantalla, i es pot desplaçar més enllà del final del document. */
static void typewriter_layout(Editor *e)
{
    int h = e->height;
    int anchor = h * e->tw_anchor / 100, limit = h * e->tw_limit / 100, row;

    if (anchor > h - 1)
        anchor = h - 1;
    if (limit > h - 1)
        limit = h - 1;
    if (limit < anchor)
        limit = anchor;
    if (e->top_blank > anchor)   /* p. ex. si la pantalla s'ha fet més petita */
        e->top_blank = anchor;
    if (e->recenter) {
        e->recenter = 0;
        put_cursor_at(e, anchor);
        return;
    }
    row = cursor_screen_row(e, limit);
    if (e->tw_mode == TW_FIXA ? row != anchor : row < 0 || row > limit)
        put_cursor_at(e, anchor);
}

void ed_layout(Editor *e, int screen_cols, int screen_lines)
{
    size_t cr, tl, tr;
    int ml = e->margin_left, avail = screen_cols - ml - e->margin_right;

    /* Si amb els marges no hi caben 20 columnes, el text ocupa tota l'amplada */
    if (avail < 20 && avail < e->columns) {
        ml = 0;
        avail = screen_cols;
    }
    e->width = e->columns < avail ? e->columns : avail;
    if (e->width < 1)
        e->width = 1;
    e->height = screen_lines - 2 > 0 ? screen_lines - 2 : 1;
    e->margin = ml + (e->center && avail > e->width ? (avail - e->width) / 2 : 0);

    /* La fila de dalt pot haver quedat fora de rang si ha canviat l'amplada */
    if (e->top_line >= e->buf.nlines)
        e->top_line = e->buf.nlines - 1;
    if (e->top_row >= screen_rows(e, e->top_line))
        e->top_row = screen_rows(e, e->top_line) - 1;

    if (e->tw_mode != TW_CLASSIC) {
        typewriter_layout(e);
        return;
    }
    e->recenter = 0;
    /* Les files en blanc d'un document nou (vegeu ed_open) es gasten una a
     * una quan el cursor arriba a baix */
    if (e->top_blank > NEW_DOC_BLANK || e->top_line || e->top_row)
        e->top_blank = 0;
    while (e->top_blank > 0 && cursor_screen_row(e, e->height - 1) > e->height - 1)
        e->top_blank--;
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
        if (e->top_blank > 0)
            e->top_blank--;
        else
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

/* ---- Selecció ---- */

static Pos cursor(const Editor *e)
{
    return (Pos){ e->cy, e->cx };
}

static void set_cursor(Editor *e, Pos p)
{
    e->cy = p.line;
    e->cx = p.col;
    e->want_x = -1;
}

int ed_selection(Editor *e, Pos *a, Pos *z)
{
    Pos c = cursor(e);

    if (e->sel_mode == SEL_NONE)
        return 0;
    if (vpos_before(c.line, c.col, e->anchor.line, e->anchor.col)) {
        *a = c;
        *z = e->anchor;
    } else {
        *a = e->anchor;
        *z = c;
    }
    return a->line != z->line || a->col != z->col;
}

void ed_sel_start(Editor *e, int mode)
{
    e->sel_mode = mode;
    e->anchor = cursor(e);
}

void ed_sel_clear(Editor *e)
{
    e->sel_mode = SEL_NONE;
}

void ed_select_all(Editor *e)
{
    ed_doc_start(e);
    ed_sel_start(e, SEL_MARK);
    ed_doc_end(e);
}

/* ---- Edició ---- */

static int delete_selection(Editor *e)
{
    Pos a, z;
    int had = ed_selection(e, &a, &z);

    ed_sel_clear(e);
    if (!had)
        return 0;
    buf_delete(&e->buf, a, z);
    set_cursor(e, a);
    return 1;
}

void ed_insert(Editor *e, const char *s, size_t n)
{
    delete_selection(e);
    set_cursor(e, buf_insert(&e->buf, cursor(e), s, n));
}

void ed_newline(Editor *e)
{
    Line *l;
    size_t p;
    char prefix[256];

    delete_selection(e);
    if (e->buf.plain) {
        ed_insert(e, "\n", 1);
        return;
    }
    l = &e->buf.lines[e->cy];
    p = md_prefix(l->text, l->len);

    /* Enter en un element de llista o cita buit: s'acaba la llista */
    if (p == l->len && e->cx == l->len && md_marker(l->text, l->len)) {
        buf_delete(&e->buf, (Pos){ e->cy, 0 }, cursor(e));
        set_cursor(e, (Pos){ e->cy, 0 });
        return;
    }
    if (p > 0 && e->cx >= p) {
        prefix[0] = '\n';
        p = 1 + md_continue(l->text, l->len, prefix + 1, sizeof prefix - 1);
        ed_insert(e, prefix, p);
    } else {
        ed_insert(e, "\n", 1);
    }
}

void ed_backspace(Editor *e)
{
    Pos c = cursor(e), a;

    if (delete_selection(e))
        return;
    if (c.col > 0)
        a = (Pos){ c.line, utf8_prev_cluster(e->buf.lines[c.line].text, c.col) };
    else if (c.line > 0)
        a = (Pos){ c.line - 1, e->buf.lines[c.line - 1].len };
    else
        return;
    buf_delete(&e->buf, a, c);
    set_cursor(e, a);
}

void ed_delete(Editor *e)
{
    Pos c = cursor(e), z;
    Line *l = &e->buf.lines[c.line];

    if (delete_selection(e))
        return;
    if (c.col < l->len)
        z = (Pos){ c.line, utf8_next_cluster(l->text, l->len, c.col) };
    else if (c.line + 1 < e->buf.nlines)
        z = (Pos){ c.line + 1, 0 };
    else
        return;
    buf_delete(&e->buf, c, z);
}

void ed_delete_word_prev(Editor *e)
{
    Pos z = cursor(e), a;

    if (delete_selection(e))
        return;
    ed_word_prev(e);
    a = cursor(e);
    if (a.line != z.line || a.col != z.col)
        buf_delete(&e->buf, a, z);
}

void ed_delete_word_next(Editor *e)
{
    Pos a = cursor(e), z;

    if (delete_selection(e))
        return;
    ed_word_next(e);
    z = cursor(e);
    set_cursor(e, a);
    if (a.line != z.line || a.col != z.col)
        buf_delete(&e->buf, a, z);
}

/* ---- Porta-retalls ---- */

size_t ed_copy(Editor *e)
{
    Pos a, z;

    if (!ed_selection(e, &a, &z))
        return 0;
    free(e->clip);
    e->clip = buf_extract(&e->buf, a, z, &e->clip_len);
    ed_sel_clear(e);
    set_cursor(e, z);   /* el cursor queda al final del bloc copiat */
    return utf8_count(e->clip, e->clip_len);
}

size_t ed_cut(Editor *e)
{
    Pos a, z;
    size_t n;

    if (!ed_selection(e, &a, &z))
        return 0;
    n = ed_copy(e);
    buf_delete(&e->buf, a, z);
    set_cursor(e, a);
    return n;
}

size_t ed_paste(Editor *e)
{
    if (!e->clip_len)
        return 0;
    ed_insert(e, e->clip, e->clip_len);
    return utf8_count(e->clip, e->clip_len);
}

/* ---- Desfer/refer ---- */

/* ---- Enfocament ---- */

static int sentence_end(wchar_t c)
{
    return c == L'.' || c == L'!' || c == L'?' || c == L'…';
}

static int sentence_close(wchar_t c)
{
    return c == L')' || c == L'»' || c == L'"' || c == L'”' || c == L'’' || c == L'\'' ||
           c == L'*' || c == L'_';
}

void text_sentence(const char *s, size_t len, size_t pos, size_t *a, size_t *b)
{
    size_t start = 0, i = 0;

    while (i < len) {
        wchar_t c;
        size_t j;

        i += utf8_decode(s, len, i, &c);
        if (!sentence_end(c))
            continue;
        /* Més signes i tancaments: «...», «?!», «.»», «.)» */
        for (j = i; j < len;) {
            size_t n = utf8_decode(s, len, j, &c);

            if (!sentence_end(c) && !sentence_close(c))
                break;
            j += n;
        }
        if (j < len && s[j] != ' ' && s[j] != '\t') {
            i = j;
            continue;   /* «3.5», «Sr.Fogg»: no acaba */
        }
        while (j < len && (s[j] == ' ' || s[j] == '\t'))
            j++;
        if (pos < j || j == len) {
            *a = start;
            *b = j;
            return;
        }
        start = i = j;
    }
    *a = start;
    *b = len;
}

void ed_focus_range(Editor *e, size_t *a, size_t *b)
{
    Line *l = &e->buf.lines[e->cy];

    if (e->focus == FOCUS_SENTENCE) {
        text_sentence(l->text, l->len, e->cx, a, b);
        /* Escrivint després del final d'una frase, en comença una de nova */
        if (e->cx == l->len && *b == l->len && *a < *b && l->len > 0) {
            wchar_t c;

            utf8_decode(l->text, l->len, utf8_prev(l->text, l->len), &c);
            if (c == L' ' || c == L'\t') {
                size_t k = l->len;

                while (k > 0 && (l->text[k - 1] == ' ' || l->text[k - 1] == '\t'))
                    k--;
                if (k > 0) {
                    utf8_decode(l->text, l->len, utf8_prev(l->text, k), &c);
                    if (sentence_end(c) || sentence_close(c))
                        *a = *b = l->len;
                }
            }
        }
    } else {
        *a = 0;
        *b = l->len;
    }
}

/* ---- Tipografia ---- */

/* La línia permet substitucions a la posició q? */
static int typo_allowed(Editor *e, size_t q)
{
    Line *l = &e->buf.lines[e->cy];
    size_t ticks = 0, i = 0;
    int k;

    if (e->buf.plain)
        return 0;
    buf_scan(&e->buf);
    k = MD_BASE(l->kind);
    while (i < l->len && (l->text[i] == ' ' || l->text[i] == '\t'))
        i++;
    /* Un paràgraf que comença amb «<<» encara és HTML per al Markdown */
    if (k == MD_HTML && !(i + 1 < l->len && l->text[i] == '<' && l->text[i + 1] == '<'))
        return 0;
    /* Les línies de guions no hi són: «-- » a l'inici d'un paràgraf encara
     * sembla un subratllat mentre s'escriu, i «---» ja el protegeix la regla
     * del tercer guió */
    if (k == MD_CODE || k == MD_FENCE_OPEN || k == MD_FENCE_CLOSE || k == MD_META ||
        k == MD_TABLE)
        return 0;
    /* Codi en línia: abans de q hi ha un accent greu sense tancar (mentre
     * s'escriu, encara no hi ha el de tancament) */
    for (i = 0; i < q && i < l->len; i++)
        ticks += l->text[i] == '`';
    return ticks % 2 == 0;
}

/* Substitueix [a, z) de la línia del cursor per rep, en un pas de desfer
 * propi; el cursor es queda darrere del mateix text. */
static void typo_replace(Editor *e, size_t a, size_t z, const char *rep)
{
    size_t n = strlen(rep);

    buf_undo_mark(&e->buf);
    buf_delete(&e->buf, (Pos){ e->cy, a }, (Pos){ e->cy, z });
    buf_insert(&e->buf, (Pos){ e->cy, a }, rep, n);
    e->cx = e->cx - (z - a) + n;
    e->want_x = -1;
    e->last_act = ACT_NONE;   /* el que s'escrigui després va en un altre pas */
}

int ed_typography(Editor *e, wchar_t ch)
{
    Line *l = &e->buf.lines[e->cy];
    const char *s = l->text;
    char enc[4];
    size_t n = utf8_encode(ch, enc), q;

    if (!e->typo || e->cx < n || memcmp(s + e->cx - n, enc, n))
        return 0;
    q = e->cx - n;   /* on és el caràcter escrit */

    /* «--» i un altre caràcter: guió llarg. Ni «---» ni el final d'un
     * comentari HTML («-->»). */
    if (ch != L'-' && ch != L'>' && q >= 2 && s[q - 1] == '-' && s[q - 2] == '-' &&
        (q < 3 || s[q - 3] != '-') && typo_allowed(e, q - 2)) {
        typo_replace(e, q - 2, q, "—");
        return 1;
    }
    if ((ch == L'<' || ch == L'>') && q >= 1 && s[q - 1] == (char)ch &&
        (q < 2 || s[q - 2] != (char)ch) && typo_allowed(e, q - 1)) {
        /* «>>» al principi de la línia és una cita niada */
        if (ch == L'>') {
            size_t k = 0;

            while (k < q - 1 && (s[k] == '>' || s[k] == ' ' || s[k] == '\t'))
                k++;
            if (k == q - 1)
                return 0;
        }
        typo_replace(e, q - 1, q + 1, ch == L'<' ? "«" : "»");
        return 1;
    }
    if (ch == L'.' && q >= 2 && s[q - 1] == '.' && s[q - 2] == '.' &&
        (q < 3 || s[q - 3] != '.') && typo_allowed(e, q - 2)) {
        typo_replace(e, q - 2, q + 1, "…");
        return 1;
    }
    if (ch == L'\'' && e->typo_apos && q >= 1) {
        wchar_t prev;

        utf8_decode(s, l->len, utf8_prev(s, q), &prev);
        if (prev != UTF8_INVALID && iswalpha((wint_t)prev) && typo_allowed(e, q)) {
            typo_replace(e, q, q + 1, "’");
            return 1;
        }
    }
    return 0;
}

/* ---- Moure paràgrafs i seccions ---- */

void ed_swap_lines(Editor *e, size_t a, size_t b, size_t c)
{
    Buffer *bf = &e->buf;
    size_t na, nb;
    char *ta = buf_extract(bf, (Pos){ a, 0 }, (Pos){ b - 1, bf->lines[b - 1].len }, &na);
    char *tb = buf_extract(bf, (Pos){ b, 0 }, (Pos){ c - 1, bf->lines[c - 1].len }, &nb);
    char *t = xmalloc(na + nb + 2);

    memcpy(t, tb, nb);
    t[nb] = '\n';
    memcpy(t + nb + 1, ta, na);
    buf_delete(bf, (Pos){ a, 0 }, (Pos){ c - 1, bf->lines[c - 1].len });
    buf_insert(bf, (Pos){ a, 0 }, t, na + nb + 1);
    free(ta);
    free(tb);
    free(t);
}

int ed_move_par(Editor *e, int dir)
{
    Pos a, z;
    size_t s = e->cy, t = e->cy;
    int sel = ed_selection(e, &a, &z);

    if (sel) {
        s = a.line;
        /* Una selecció de línies senceres acaba a l'inici de la següent */
        t = z.col == 0 && z.line > a.line ? z.line - 1 : z.line;
    }
    if (dir < 0 ? s == 0 : t + 1 >= e->buf.nlines)
        return 0;
    if (dir < 0)
        ed_swap_lines(e, s - 1, s, t + 1);
    else
        ed_swap_lines(e, s, t + 1, t + 2);
    if (dir < 0)
        s--, t--;
    else
        s++, t++;
    if (sel) {
        /* La selecció passa a ser el bloc sencer, que es pot tornar a moure */
        e->anchor = (Pos){ s, 0 };
        e->cy = t;
        e->cx = e->buf.lines[t].len;
    } else {
        e->cy = s;   /* la columna no canvia: la línia és la mateixa */
    }
    e->want_x = -1;
    return 1;
}

void ed_begin(Editor *e, int act, wchar_t ch)
{
    int same = act != ACT_NONE && act == e->last_act && e->sel_mode == SEL_NONE &&
               e->cy == e->act_end.line && e->cx == e->act_end.col;

    /* Escrivint, cada paraula és un pas de desfer */
    if (same && act == ACT_TYPE && ch != L' ' && e->last_ch == L' ')
        same = 0;
    if (!same)
        buf_undo_mark(&e->buf);
    e->last_act = act;
    e->last_ch = ch;
}

void ed_end(Editor *e)
{
    e->act_end = cursor(e);
}

int ed_undo(Editor *e)
{
    Pos c;

    if (!buf_undo(&e->buf, &c))
        return 0;
    ed_sel_clear(e);
    set_cursor(e, c);
    return 1;
}

int ed_redo(Editor *e)
{
    Pos c;

    if (!buf_redo(&e->buf, &c))
        return 0;
    ed_sel_clear(e);
    set_cursor(e, c);
    return 1;
}

void ed_select(Editor *e, Pos a, Pos z)
{
    set_cursor(e, a);
    ed_sel_start(e, SEL_SHIFT);
    set_cursor(e, z);
}

void theme_apply(Editor *e, const Theme *t)
{
    theme_palette(t, e->palette);
    e->theme = theme_index(t);
}
