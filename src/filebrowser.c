#include "filebrowser.h"
#include "dirlist.h"
#include "keys.h"
#include "tui.h"
#include "utf8.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define PARENT ((size_t)-1)   /* l'entrada ".." */

typedef struct {
    char    dir[PATH_MAX];     /* absolut i normalitzat */
    DirList list;
    size_t *shown;             /* índexs de list que passen el filtre (o PARENT) */
    size_t  nshown;
    size_t  sel, top;
    char    filter[256];
    char    msg[256];
} Browser;

static char last_dir[PATH_MAX];
static int  show_hidden;

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

static int str_width(const char *s)
{
    size_t len = strlen(s), i = 0;
    int col = 0;

    while (i < len) {
        wchar_t wc;

        i += utf8_decode(s, len, i, &wc);
        col += utf8_width(utf8_printable(wc) ? wc : L'?', col);
    }
    return col;
}

/* Com put_str, però si no hi cap, en mostra el final precedit de "…": en
 * un camí llarg, el que interessa és el directori actual. */
static void put_tail(const char *s, int maxw)
{
    size_t len = strlen(s), i = 0;

    if (str_width(s) <= maxw || maxw < 2) {
        put_str(s, maxw);
        return;
    }
    while (i < len && str_width(s + i) > maxw - 1)
        i = utf8_next(s, len, i);
    addnwstr(L"…", 1);
    put_str(s + i, maxw - 1);
}

static void refilter(Browser *b)
{
    b->nshown = 0;
    free(b->shown);
    b->shown = malloc((b->list.n + 1) * sizeof *b->shown);
    if (!b->shown)
        return;
    if (!b->filter[0] && strcmp(b->dir, "/"))
        b->shown[b->nshown++] = PARENT;
    for (size_t i = 0; i < b->list.n; i++)
        if (dirlist_match(b->list.items[i].name, b->filter))
            b->shown[b->nshown++] = i;
    if (b->sel >= b->nshown)
        b->sel = b->nshown ? b->nshown - 1 : 0;
}

/* Entra al directori dir i hi selecciona l'entrada select (si n'hi ha). */
static int change_dir(Browser *b, const char *dir, const char *select)
{
    char real[PATH_MAX];
    DirList nl = {0};

    if (!realpath(dir, real) || dirlist_read(&nl, real, show_hidden) < 0) {
        snprintf(b->msg, sizeof b->msg, "No es pot obrir %s: %s", dir, strerror(errno));
        dirlist_free(&nl);
        return -1;
    }
    dirlist_free(&b->list);
    b->list = nl;
    snprintf(b->dir, sizeof b->dir, "%s", real);
    b->filter[0] = '\0';
    b->sel = b->top = 0;
    refilter(b);
    for (size_t i = 0; select && i < b->nshown; i++)
        if (b->shown[i] != PARENT && !strcmp(b->list.items[b->shown[i]].name, select))
            b->sel = i;
    return 0;
}

static void reload(Browser *b)
{
    char dir[PATH_MAX], name[256] = "";
    char filter[sizeof b->filter];

    if (b->nshown && b->shown[b->sel] != PARENT)
        snprintf(name, sizeof name, "%s", b->list.items[b->shown[b->sel]].name);
    snprintf(dir, sizeof dir, "%s", b->dir);
    snprintf(filter, sizeof filter, "%s", b->filter);
    if (change_dir(b, dir, name) == 0 && filter[0]) {
        snprintf(b->filter, sizeof b->filter, "%s", filter);
        refilter(b);
    }
}

static void go_parent(Browser *b)
{
    char parent[PATH_MAX];
    const char *slash = strrchr(b->dir, '/');
    char name[256];

    if (!strcmp(b->dir, "/"))
        return;
    snprintf(name, sizeof name, "%s", slash ? slash + 1 : "");
    path_parent(b->dir, parent, sizeof parent);
    change_dir(b, parent, name);
}

/* El camí dir/name amb "~" en lloc del directori personal. */
static void pretty_path(const char *path, char *out, size_t outsz)
{
    const char *home = getenv("HOME");
    size_t n = home ? strlen(home) : 0;

    if (n > 1 && !strncmp(path, home, n) && (path[n] == '/' || path[n] == '\0'))
        snprintf(out, outsz, "~%s", path + n);
    else
        snprintf(out, outsz, "%s", path);
}

/* Expandeix "~" al principi de s. */
static void expand_home(const char *s, char *out, size_t outsz)
{
    const char *home = getenv("HOME");

    if (s[0] == '~' && (s[1] == '/' || s[1] == '\0') && home)
        snprintf(out, outsz, "%s%s", home, s + 1);
    else
        snprintf(out, outsz, "%s", s);
}

static void draw_entry(Browser *b, size_t idx, int y, int selected)
{
    char right[64] = "", size[16], date[32];
    int rw, w;

    move(y, 0);
    if (selected)
        attrset(ui_attr(UI_SEL));
    hline(' ', COLS);
    move(y, 1);
    if (idx == PARENT) {
        attron(A_BOLD);
        put_str("../", COLS - 2);
        attroff(A_BOLD);
    } else {
        const DirEntry *it = &b->list.items[idx];
        struct tm tm;

        localtime_r(&it->mtime, &tm);
        strftime(date, sizeof date, "%d/%m/%Y %H:%M", &tm);
        format_size(it->size, size, sizeof size);
        if (COLS >= 50)
            snprintf(right, sizeof right, "%8s  %s", it->is_dir ? "" : size, date);
        else if (COLS >= 30 && !it->is_dir)
            snprintf(right, sizeof right, "%8s", size);
        rw = str_width(right);
        if (it->is_dir)
            attron(A_BOLD);
        w = put_str(it->name, COLS - 3 - rw);
        if (it->is_dir && w < COLS - 3 - rw)
            addch('/');
        if (it->is_dir)
            attroff(A_BOLD);
        if (rw)
            mvaddstr(y, COLS - 1 - rw, right);
    }
    attrset(A_NORMAL);
}

static void draw(Browser *b)
{
    int h = LINES - 3;
    char path[PATH_MAX + 16];

    if (h < 1)
        h = 1;
    if (b->sel < b->top)
        b->top = b->sel;
    if (b->sel >= b->top + (size_t)h)
        b->top = b->sel - (size_t)h + 1;

    erase();
    attrset(ui_attr(UI_BAR));
    mvhline(0, 0, ' ', COLS);
    pretty_path(b->dir, path, sizeof path);
    move(0, 1);
    put_str("Obrir: ", COLS - 1);
    put_tail(path, COLS - 9);
    attrset(A_NORMAL);

    for (int y = 0; y < h && b->top + (size_t)y < b->nshown; y++)
        draw_entry(b, b->shown[b->top + (size_t)y], y + 1, b->top + (size_t)y == b->sel);
    if (b->nshown == 0)
        mvaddnstr(1, 1, b->filter[0] ? "(cap coincidència: Enter crea un fitxer nou amb aquest nom)"
                                     : "(directori buit)", COLS - 2);

    attrset(ui_attr(UI_BAR));
    mvhline(LINES - 2, 0, ' ', COLS);
    move(LINES - 2, 1);
    put_str(show_hidden ? "Enter obre · ← pare · escriu per filtrar · Ctrl+A amaga ocults · Esc surt"
                        : "Enter obre · ← pare · escriu per filtrar · Ctrl+A mostra ocults · Esc surt",
            COLS - 2);
    attrset(A_NORMAL);

    move(LINES - 1, 0);
    if (b->msg[0]) {
        put_str(b->msg, COLS);
    } else {
        attron(A_BOLD);
        put_str("Filtre: ", COLS);
        attroff(A_BOLD);
        put_str(b->filter, COLS - 8);
    }
}

/* Camí relatiu al directori de treball si hi és a dins (més curt a la barra d'estat). */
static void shorten(const char *path, char *out, size_t outsz)
{
    char cwd[PATH_MAX];
    size_t n;

    if (getcwd(cwd, sizeof cwd) && (n = strlen(cwd)) > 1 && !strncmp(path, cwd, n) &&
        path[n] == '/')
        snprintf(out, outsz, "%s", path + n + 1);
    else
        snprintf(out, outsz, "%s", path);
}

/* Enter: 1 si s'ha triat un fitxer (a out). */
static int activate(Browser *b, char *out, size_t outsz)
{
    char path[PATH_MAX], full[PATH_MAX];
    struct stat st;

    if (b->filter[0] == '/' || b->filter[0] == '~') {
        /* Camí escrit a mà */
        expand_home(b->filter, path, sizeof path);
        if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
            change_dir(b, path, NULL);
            return 0;
        }
        shorten(path, out, outsz);
        return 1;
    }
    if (b->nshown == 0) {
        if (!b->filter[0])
            return 0;
        if (strchr(b->filter, '/')) {
            snprintf(b->msg, sizeof b->msg, "El nom no pot contenir '/'");
            return 0;
        }
        path_join(b->dir, b->filter, full, sizeof full);
        shorten(full, out, outsz);
        return 1;
    }
    if (b->shown[b->sel] == PARENT) {
        go_parent(b);
        return 0;
    }
    path_join(b->dir, b->list.items[b->shown[b->sel]].name, full, sizeof full);
    if (b->list.items[b->shown[b->sel]].is_dir) {
        change_dir(b, full, NULL);
        return 0;
    }
    shorten(full, out, outsz);
    return 1;
}

static void start_dir(Editor *e, char *dir, size_t dirsz, char *select, size_t selsz)
{
    char real[PATH_MAX];

    select[0] = '\0';
    if (e->filename) {
        const char *slash;

        if (realpath(e->filename, real)) {
            slash = strrchr(real, '/');
            snprintf(select, selsz, "%s", slash + 1);
            path_parent(real, dir, dirsz);
            return;
        }
        /* Fitxer nou encara no desat: el seu directori, si existeix */
        snprintf(real, sizeof real, "%s", e->filename);
        if ((slash = strrchr(real, '/')) && slash > real) {
            real[slash - real] = '\0';
            if (access(real, X_OK) == 0) {
                snprintf(dir, dirsz, "%s", real);
                return;
            }
        }
    }
    if (last_dir[0] && access(last_dir, X_OK) == 0)
        snprintf(dir, dirsz, "%s", last_dir);
    else if (!getcwd(dir, dirsz))
        snprintf(dir, dirsz, "/");
}

int file_browser(Editor *e, char *out, size_t outsz)
{
    Browser b;
    char dir[PATH_MAX], select[256];
    int done = 0, chosen = 0;
    size_t len;

    memset(&b, 0, sizeof b);
    start_dir(e, dir, sizeof dir, select, sizeof select);
    if (change_dir(&b, dir, select[0] ? select : NULL) < 0 && change_dir(&b, "/", NULL) < 0) {
        ed_msg(e, "%s", b.msg);
        return 0;
    }

    key_set_timeout(-1);
    while (!done) {
        size_t page = LINES > 4 ? (size_t)LINES - 4 : 1;
        int key;

        draw(&b);
        refresh();
        key = key_read();
        if (key < 0)
            continue;
        b.msg[0] = '\0';
        switch (key) {
        case KEY_FLAG_FN | KEY_UP:
            if (b.sel > 0)
                b.sel--;
            break;
        case KEY_FLAG_FN | KEY_DOWN:
            if (b.sel + 1 < b.nshown)
                b.sel++;
            break;
        case KEY_FLAG_FN | KEY_PPAGE:
            b.sel = b.sel > page ? b.sel - page : 0;
            break;
        case KEY_FLAG_FN | KEY_NPAGE:
            b.sel = b.sel + page < b.nshown ? b.sel + page : (b.nshown ? b.nshown - 1 : 0);
            break;
        case KEY_FLAG_FN | KEY_HOME:
            b.sel = 0;
            break;
        case KEY_FLAG_FN | KEY_END:
            b.sel = b.nshown ? b.nshown - 1 : 0;
            break;
        case KEY_FLAG_FN | KEY_LEFT:
            go_parent(&b);
            break;
        case KEY_FLAG_FN | KEY_RIGHT:
            if (b.nshown && b.shown[b.sel] != PARENT && b.list.items[b.shown[b.sel]].is_dir)
                activate(&b, out, outsz);
            break;
        case '\n':
            if (activate(&b, out, outsz))
                done = chosen = 1;
            break;
        case KEY_FLAG_FN | KEY_BACKSPACE:
        case 8:
            if ((len = strlen(b.filter)) > 0) {
                b.filter[utf8_prev(b.filter, len)] = '\0';
                b.sel = 0;
                refilter(&b);
            } else {
                go_parent(&b);
            }
            break;
        case 27:
            if (b.filter[0]) {
                b.filter[0] = '\0';
                refilter(&b);
            } else {
                done = 1;
            }
            break;
        case 1:   /* Ctrl+A */
            show_hidden = !show_hidden;
            reload(&b);
            break;
        default:
            if (!(key & (KEY_FLAG_FN | KEY_FLAG_ALT)) && utf8_printable((wchar_t)key)) {
                char s[4];
                size_t n = utf8_encode((wchar_t)key, s);

                if ((len = strlen(b.filter)) + n < sizeof b.filter) {
                    memcpy(b.filter + len, s, n);
                    b.filter[len + n] = '\0';
                    b.sel = 0;
                    refilter(&b);
                }
            }
        }
    }
    snprintf(last_dir, sizeof last_dir, "%s", b.dir);
    dirlist_free(&b.list);
    free(b.shown);
    return chosen;
}
