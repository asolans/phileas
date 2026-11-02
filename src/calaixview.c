#include "calaixview.h"
#include "calaix.h"
#include "keys.h"
#include "tui.h"
#include "utf8.h"
#include "util.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    Calaix  c;
    size_t *shown;           /* índexs de les entrades que es mostren */
    size_t  nshown;
    size_t  sel, top;
    char    filter[256];
    char    msg[256];
} View;

/* Escriu els len primers bytes de s a partir de la posició actual sense
 * passar de maxw columnes. Retorna les columnes ocupades. */
static int put_str_n(const char *s, size_t len, int maxw)
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

static int put_str(const char *s, int maxw)
{
    return put_str_n(s, strlen(s), maxw);
}

static void refilter(View *v)
{
    v->nshown = 0;
    for (size_t i = 0; i < v->c.n; i++)
        if (calaix_match(&v->c, i, v->filter))
            v->shown[v->nshown++] = i;
    if (v->sel >= v->nshown)
        v->sel = v->nshown ? v->nshown - 1 : 0;
}

/* La capçalera sense el «## » inicial */
static void draw_entry(View *v, size_t idx, int y, int selected)
{
    size_t hl;
    const char *head = calaix_head(&v->c, idx, &hl);

    move(y, 0);
    if (selected)
        attrset(ui_attr(UI_SEL));
    hline(' ', COLS);
    move(y, 1);
    put_str_n(head + 3, hl - 3, COLS - 2);
    attrset(A_NORMAL);
}

static void draw(View *v)
{
    int h = LINES - 3;

    if (h < 1)
        h = 1;
    if (v->sel < v->top)
        v->top = v->sel;
    if (v->sel >= v->top + (size_t)h)
        v->top = v->sel - (size_t)h + 1;

    erase();
    attrset(ui_attr(UI_BAR));
    mvhline(0, 0, ' ', COLS);
    move(0, 1);
    put_str("Calaix d'idees", COLS - 2);
    attrset(A_NORMAL);

    for (int y = 0; y < h && v->top + (size_t)y < v->nshown; y++)
        draw_entry(v, v->shown[v->top + (size_t)y], y + 1, v->top + (size_t)y == v->sel);
    if (v->nshown == 0)
        mvaddnstr(1, 1, "(cap entrada no coincideix amb el filtre)", COLS - 2);

    attrset(ui_attr(UI_BAR));
    mvhline(LINES - 2, 0, ' ', COLS);
    move(LINES - 2, 1);
    put_str("Enter va a l'entrada · escriu per filtrar (etiqueta o text) · Esc surt", COLS - 2);
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

int calaix_view(Editor *e, char *out, size_t outsz)
{
    View v;
    size_t len;
    int done = 0, chosen = 0;

    memset(&v, 0, sizeof v);
    if (calaix_load(e->calaix, &v.c) < 0) {
        ed_msg(e, "No s'ha pogut llegir %s: %s", e->calaix, strerror(errno));
        return 0;
    }
    if (v.c.n == 0) {
        ed_msg(e, "El calaix és buit");
        calaix_free(&v.c);
        return 0;
    }
    v.shown = xmalloc(v.c.n * sizeof *v.shown);
    refilter(&v);

    key_set_timeout(-1);
    while (!done) {
        size_t page = LINES > 4 ? (size_t)LINES - 4 : 1;
        int key;

        draw(&v);
        refresh();
        key = key_read();
        if (key < 0)
            continue;
        v.msg[0] = '\0';
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
                size_t hl;
                const char *head = calaix_head(&v.c, v.shown[v.sel], &hl);

                snprintf(out, outsz, "%.*s", (int)hl, head);
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
    calaix_free(&v.c);
    free(v.shown);
    return chosen;
}
