#include "outlineview.h"
#include "command.h"
#include "dirlist.h"
#include "keys.h"
#include "outline.h"
#include "tui.h"
#include "utf8.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define START ((size_t)-1)   /* l'entrada del text d'abans de la primera capçalera */

typedef struct {
    Outline o;
    size_t *shown;           /* índexs de o.items que passen el filtre (o START) */
    size_t  nshown;
    size_t  sel, top;
    char    filter[256];
    char    msg[256];
} View;

/* Escriu s a partir de la posició actual sense passar de maxw columnes.
 * Retorna les columnes ocupades. */
static int put_str(const char *s, int maxw)
{
    size_t len = strlen(s), i = 0;
    int col = 0;

    while (i < len) {
        wchar_t wc;
        size_t n = utf8_decode(s, len, i, &wc);
        int w;

        if (!utf8_printable(wc))
            wc = L'?';
        w = utf8_width(wc, col);
        if (col + w > maxw)
            break;
        addnwstr(&wc, 1);
        col += w;
        i += n;
    }
    return col;
}

static void refilter(View *v)
{
    v->nshown = 0;
    if (!v->filter[0] && v->o.first_words)
        v->shown[v->nshown++] = START;
    for (size_t i = 0; i < v->o.n; i++)
        if (dirlist_match(v->o.items[i].title, v->filter))
            v->shown[v->nshown++] = i;
    if (v->sel >= v->nshown)
        v->sel = v->nshown ? v->nshown - 1 : 0;
}

static void draw_entry(View *v, size_t idx, int y, int selected)
{
    char right[32];
    size_t words = idx == START ? v->o.first_words : v->o.items[idx].words;
    int rw, indent;

    move(y, 0);
    if (selected)
        attrset(ui_attr(UI_SEL));
    hline(' ', COLS);
    snprintf(right, sizeof right, "%zu", words);
    rw = (int)strlen(right);
    if (idx == START) {
        move(y, 1);
        attron(A_DIM);
        put_str("(inici del document)", COLS - 3 - rw);
        attroff(A_DIM);
    } else {
        const OutlineItem *it = &v->o.items[idx];

        indent = 2 * (it->level - 1);
        if (indent > COLS / 3)
            indent = COLS / 3;
        move(y, 1 + indent);
        if (it->level == 1)
            attron(A_BOLD);
        if (it->title[0]) {
            put_str(it->title, COLS - 3 - rw - indent);
        } else {
            attron(A_DIM);
            put_str("(sense títol)", COLS - 3 - rw - indent);
            attroff(A_DIM);
        }
        attroff(A_BOLD);
    }
    if (COLS > rw + 10)
        mvaddstr(y, COLS - 1 - rw, right);
    attrset(A_NORMAL);
}

static void draw(View *v, Editor *e, size_t total)
{
    int h = LINES - 3;
    char title[300];

    if (h < 1)
        h = 1;
    if (v->sel < v->top)
        v->top = v->sel;
    if (v->sel >= v->top + (size_t)h)
        v->top = v->sel - (size_t)h + 1;

    erase();
    attrset(ui_attr(UI_BAR));
    mvhline(0, 0, ' ', COLS);
    snprintf(title, sizeof title, "Esquema: %s · %zu %s", e->filename ? e->filename : "(sense nom)",
             total, total == 1 ? "paraula" : "paraules");
    move(0, 1);
    put_str(title, COLS - 2);
    attrset(A_NORMAL);

    for (int y = 0; y < h && v->top + (size_t)y < v->nshown; y++)
        draw_entry(v, v->shown[v->top + (size_t)y], y + 1, v->top + (size_t)y == v->sel);
    if (v->nshown == 0)
        mvaddnstr(1, 1, "(cap capçalera conté aquest text)", COLS - 2);

    attrset(ui_attr(UI_BAR));
    mvhline(LINES - 2, 0, ' ', COLS);
    move(LINES - 2, 1);
    put_str("Enter hi va · Alt+↑↓ mou la secció · escriu per filtrar · Esc surt", COLS - 2);
    attrset(A_NORMAL);

    move(LINES - 1, 0);
    if (v->msg[0]) {
        put_str(v->msg, COLS);
    } else {
        attron(A_BOLD);
        put_str("Filtre: ", COLS);
        attroff(A_BOLD);
        put_str(v->filter, COLS - 8);
    }
}

/* Mou la secció seleccionada amb les seves subseccions. El cursor del
 * document la segueix, per si se surt amb Esc. */
static void move_section(View *v, Editor *e, int dir)
{
    size_t k, a, b, c, head, end = e->buf.nlines;

    if (!v->nshown || v->shown[v->sel] == START) {
        snprintf(v->msg, sizeof v->msg, "El text d'abans de la primera capçalera no es mou");
        return;
    }
    k = v->shown[v->sel];
    /* Les línies buides del final es queden al final */
    while (end > 1 && e->buf.lines[end - 1].len == 0)
        end--;
    if (!outline_move(&v->o, k, dir, end, &a, &b, &c, &head)) {
        snprintf(v->msg, sizeof v->msg, dir < 0 ? "Ja és la primera secció del seu nivell"
                                                : "Ja és l'última secció del seu nivell");
        return;
    }
    buf_undo_mark(&e->buf);
    ed_swap_lines(e, a, b, c);
    outline_free(&v->o);
    outline_build(&e->buf, &v->o);
    refilter(v);
    for (size_t i = 0; i < v->nshown; i++)
        if (v->shown[i] != START && v->o.items[v->shown[i]].line == head)
            v->sel = i;
    ed_sel_clear(e);
    e->cy = head;
    e->cx = 0;
    e->want_x = -1;
    e->top_line = head;
    e->top_row = 0;
    e->recenter = 1;
    snprintf(v->msg, sizeof v->msg, "Secció moguda (Ctrl+Z, al document, ho desfà)");
}

int outline_view(Editor *e, size_t *line)
{
    View v;
    size_t cur, len, total;
    int done = 0, chosen = 0;

    memset(&v, 0, sizeof v);
    if (e->buf.plain) {
        ed_msg(e, "L'esquema només funciona amb documents Markdown");
        return 0;
    }
    outline_build(&e->buf, &v.o);
    if (v.o.n == 0) {
        ed_msg(e, "El document no té capçaleres («# Títol»)");
        outline_free(&v.o);
        return 0;
    }
    total = buf_words(&e->buf);
    v.shown = xmalloc((v.o.n + 1) * sizeof *v.shown);
    refilter(&v);
    cur = outline_at(&v.o, e->cy);
    for (size_t i = 0; i < v.nshown; i++)
        if (v.shown[i] == cur)
            v.sel = i;

    key_set_timeout(-1);
    while (!done) {
        size_t page = LINES > 4 ? (size_t)LINES - 4 : 1;
        const Command *c;
        int key;

        draw(&v, e, total);
        refresh();
        key = key_read();
        if (key < 0)
            continue;
        v.msg[0] = '\0';
        if ((c = cmd_for_key(key)) && (c == cmd_find("pujar-paragraf") ||
                                       c == cmd_find("baixar-paragraf"))) {
            move_section(&v, e, c == cmd_find("pujar-paragraf") ? -1 : 1);
            continue;
        }
        switch (key) {
        case KEY_FLAG_FN | KEY_UP:
            if (v.sel > 0)
                v.sel--;
            break;
        case KEY_FLAG_FN | KEY_DOWN:
            if (v.sel + 1 < v.nshown)
                v.sel++;
            break;
        case KEY_FLAG_FN | KEY_PPAGE:
            v.sel = v.sel > page ? v.sel - page : 0;
            break;
        case KEY_FLAG_FN | KEY_NPAGE:
            v.sel = v.sel + page < v.nshown ? v.sel + page : (v.nshown ? v.nshown - 1 : 0);
            break;
        case KEY_FLAG_FN | KEY_HOME:
            v.sel = 0;
            break;
        case KEY_FLAG_FN | KEY_END:
            v.sel = v.nshown ? v.nshown - 1 : 0;
            break;
        case '\n':
            if (v.nshown) {
                *line = v.shown[v.sel] == START ? 0 : v.o.items[v.shown[v.sel]].line;
                done = chosen = 1;
            }
            break;
        case KEY_FLAG_FN | KEY_BACKSPACE:
        case 8:
            if ((len = strlen(v.filter)) > 0) {
                v.filter[utf8_prev(v.filter, len)] = '\0';
                v.sel = 0;
                refilter(&v);
            }
            break;
        case 27:
            if (v.filter[0]) {
                v.filter[0] = '\0';
                refilter(&v);
            } else {
                done = 1;
            }
            break;
        default:
            if (!(key & (KEY_FLAG_FN | KEY_FLAG_ALT)) && utf8_printable((wchar_t)key)) {
                char s[4];
                size_t n = utf8_encode((wchar_t)key, s);

                if ((len = strlen(v.filter)) + n < sizeof v.filter) {
                    memcpy(v.filter + len, s, n);
                    v.filter[len + n] = '\0';
                    v.sel = 0;
                    refilter(&v);
                }
            }
        }
    }
    outline_free(&v.o);
    free(v.shown);
    return chosen;
}
