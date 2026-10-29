#include "recentview.h"
#include "dirlist.h"
#include "history.h"
#include "keys.h"
#include "tui.h"
#include "utf8.h"
#include "util.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    History h;
    size_t *shown;           /* índexs de h.items que es mostren */
    size_t  nshown;
    size_t  sel, top;
    char    filter[256];
    char    msg[256];
    char    home[PATH_MAX];  /* es mostra com a ~ */
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

static const char *base_name(const char *path)
{
    const char *s = strrchr(path, '/');

    return s ? s + 1 : path;
}

/* Directori del camí, amb ~ en lloc de la carpeta de l'usuari */
static void dir_name(View *v, const char *path, char *out, size_t size)
{
    size_t dlen = (size_t)(base_name(path) - path), hlen = strlen(v->home);

    if (dlen > 1)
        dlen--;   /* sense la barra final, excepte a l'arrel */
    if (hlen > 1 && dlen >= hlen && !strncmp(path, v->home, hlen) &&
        (dlen == hlen || path[hlen] == '/'))
        snprintf(out, size, "~%.*s", (int)(dlen - hlen), path + hlen);
    else
        snprintf(out, size, "%.*s", (int)dlen, path);
}

static void refilter(View *v)
{
    v->nshown = 0;
    for (size_t i = 0; i < v->h.n; i++)
        if (dirlist_match(v->h.items[i].path, v->filter))
            v->shown[v->nshown++] = i;
    if (v->sel >= v->nshown)
        v->sel = v->nshown ? v->nshown - 1 : 0;
}

static void draw_entry(View *v, size_t idx, int y, int selected)
{
    const char *path = v->h.items[idx].path;
    char dir[PATH_MAX + 2];
    int col;

    move(y, 0);
    if (selected)
        attrset(ui_attr(UI_SEL));
    hline(' ', COLS);
    move(y, 1);
    attron(A_BOLD);
    col = put_str(base_name(path), COLS - 2);
    attroff(A_BOLD);
    if (col + 4 < COLS - 2) {
        dir_name(v, path, dir, sizeof dir);
        move(y, 1 + col + 2);
        attron(A_DIM);
        put_str(dir, COLS - 2 - col - 2);
        attroff(A_DIM);
    }
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
    put_str("Documents recents", COLS - 2);
    attrset(A_NORMAL);

    for (int y = 0; y < h && v->top + (size_t)y < v->nshown; y++)
        draw_entry(v, v->shown[v->top + (size_t)y], y + 1, v->top + (size_t)y == v->sel);
    if (v->nshown == 0)
        mvaddnstr(1, 1, v->h.n ? "(cap document conté aquest text)" : "(la llista és buida)",
                  COLS - 2);

    attrset(ui_attr(UI_BAR));
    mvhline(LINES - 2, 0, ' ', COLS);
    move(LINES - 2, 1);
    put_str("Enter l'obre · Supr el treu de la llista · escriu per filtrar · Esc surt", COLS - 2);
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

/* Treu de la llista el document obert */
static void drop_current(View *v, const Editor *e)
{
    char cur[PATH_MAX];
    long i;

    if (e->filename && realpath(e->filename, cur) && (i = history_find(&v->h, cur)) >= 0)
        history_remove(&v->h, (size_t)i);
}

int recent_view(Editor *e, char *out, size_t outsz)
{
    View v;
    size_t len;
    int done = 0, chosen = 0;
    const char *home = getenv("HOME");

    memset(&v, 0, sizeof v);
    if (!home || !realpath(home, v.home))
        v.home[0] = '\0';
    history_load(&v.h);
    if (history_prune_missing(&v.h))
        history_save(&v.h);   /* els que ja no hi són, fora també del disc */
    drop_current(&v, e);
    if (v.h.n == 0) {
        ed_msg(e, "No hi ha cap altre document recent");
        history_free(&v.h);
        return 0;
    }
    v.shown = xmalloc(v.h.n * sizeof *v.shown);
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
                snprintf(out, outsz, "%s", v.h.items[v.shown[v.sel]].path);
                done = chosen = 1;
            }
            break;
        case KEY_FLAG_FN | KEY_DC:
            if (v.nshown) {
                /* Es treu de l'historial del disc, que té el document obert */
                const char *path = v.h.items[v.shown[v.sel]].path;
                History full;
                long i;

                history_load(&full);
                if ((i = history_find(&full, path)) >= 0)
                    history_remove(&full, (size_t)i);
                if (history_save(&full) < 0)
                    snprintf(v.msg, sizeof v.msg, "No s'ha pogut desar l'historial");
                else
                    snprintf(v.msg, sizeof v.msg, "Tret de la llista: %s (el fitxer no s'esborra)",
                             base_name(path));
                history_free(&full);
                history_remove(&v.h, v.shown[v.sel]);
                refilter(&v);
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
    history_free(&v.h);
    free(v.shown);
    return chosen;
}
