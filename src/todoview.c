#include "todoview.h"
#include "dirlist.h"
#include "keys.h"
#include "todo.h"
#include "tui.h"
#include "utf8.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    size_t *lines;           /* línies amb marcador */
    size_t  n;
    size_t *shown;           /* índexs de lines que passen el filtre */
    size_t  nshown, sel, top;
    char    filter[256];
} View;

static int put_str(const char *s, size_t len, int maxw)
{
    size_t i = 0;
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

/* Text de la línia sense espais als extrems, com a cadena nova */
static char *line_text(Editor *e, size_t y)
{
    const Line *l = &e->buf.lines[y];
    size_t a = 0, b = l->len;
    char *t;

    while (a < b && (l->text[a] == ' ' || l->text[a] == '\t'))
        a++;
    while (b > a && (l->text[b - 1] == ' ' || l->text[b - 1] == '\t'))
        b--;
    t = xmalloc(b - a + 1);
    memcpy(t, l->text + a, b - a);
    t[b - a] = '\0';
    return t;
}

static void refilter(View *v, Editor *e)
{
    v->nshown = 0;
    for (size_t i = 0; i < v->n; i++) {
        char *t = line_text(e, v->lines[i]);
        int ok = dirlist_match(t, v->filter);

        free(t);
        if (ok)
            v->shown[v->nshown++] = i;
    }
    if (v->sel >= v->nshown)
        v->sel = v->nshown ? v->nshown - 1 : 0;
}

static void draw(View *v, Editor *e)
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
    snprintf(title, sizeof title, "Pendents: %s · %zu", e->filename ? e->filename : "(sense nom)", v->n);
    move(0, 1);
    put_str(title, strlen(title), COLS - 2);
    attrset(A_NORMAL);

    for (int y = 0; y < h && v->top + (size_t)y < v->nshown; y++) {
        size_t ln = v->lines[v->shown[v->top + (size_t)y]];
        int selected = v->top + (size_t)y == v->sel;
        char num[24], *t = line_text(e, ln);
        int nw;

        snprintf(num, sizeof num, "%zu", ln + 1);
        nw = (int)strlen(num);
        move(y + 1, 0);
        if (selected)
            attrset(ui_attr(UI_SEL));
        hline(' ', COLS);
        mvaddstr(y + 1, nw < 6 ? 6 - nw : 0, num);
        move(y + 1, 8);
        put_str(t, strlen(t), COLS - 9);
        attrset(A_NORMAL);
        free(t);
    }
    if (v->nshown == 0)
        mvaddnstr(1, 1, "(cap pendent conté aquest text)", COLS - 2);

    attrset(ui_attr(UI_BAR));
    mvhline(LINES - 2, 0, ' ', COLS);
    move(LINES - 2, 1);
    put_str("Enter hi va · escriu per filtrar · Esc surt", 44, COLS - 2);
    attrset(A_NORMAL);
    move(LINES - 1, 0);
    attron(A_BOLD);
    put_str("Filtre: ", 8, COLS);
    attroff(A_BOLD);
    put_str(v->filter, strlen(v->filter), COLS - 8);
}

int todo_view(Editor *e, size_t *line)
{
    View v;
    size_t len;
    int done = 0, chosen = 0;

    memset(&v, 0, sizeof v);
    if (e->buf.plain) {
        ed_msg(e, "Els pendents només funcionen amb documents Markdown");
        return 0;
    }
    v.n = todo_collect(&e->buf, &v.lines);
    if (v.n == 0) {
        ed_msg(e, "No hi ha pendents (TODO, [?] ni %%%%nota%%%%)");
        return 0;
    }
    v.shown = xmalloc(v.n * sizeof *v.shown);
    refilter(&v, e);
    for (size_t i = 0; i < v.n; i++)
        if (v.lines[i] >= e->cy) {
            v.sel = i;
            break;
        }

    key_set_timeout(-1);
    while (!done) {
        size_t page = LINES > 4 ? (size_t)LINES - 4 : 1;
        int key;

        draw(&v, e);
        refresh();
        key = key_read();
        if (key < 0)
            continue;
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
                *line = v.lines[v.shown[v.sel]];
                done = chosen = 1;
            }
            break;
        case KEY_FLAG_FN | KEY_BACKSPACE:
        case 8:
            if ((len = strlen(v.filter)) > 0) {
                v.filter[utf8_prev(v.filter, len)] = '\0';
                v.sel = 0;
                refilter(&v, e);
            }
            break;
        case 27:
            if (v.filter[0]) {
                v.filter[0] = '\0';
                refilter(&v, e);
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
                    refilter(&v, e);
                }
            }
        }
    }
    free(v.lines);
    free(v.shown);
    return chosen;
}
