#include "history.h"
#include "util.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

void history_path(char *out, size_t size)
{
    char dir[1024];

    config_dir(dir, sizeof dir);
    snprintf(out, size, "%s/historial", dir);
}

void history_load(History *h)
{
    char path[1100], *line = NULL;
    size_t cap = 0;
    ssize_t len;
    FILE *f;

    memset(h, 0, sizeof *h);
    history_path(path, sizeof path);
    if (!(f = fopen(path, "r")))
        return;
    h->items = xmalloc(HISTORY_MAX * sizeof *h->items);
    while (h->n < HISTORY_MAX && (len = getline(&line, &cap, f)) > 0) {
        unsigned long long y, x;
        int off = 0;

        if (line[len - 1] == '\n')
            line[--len] = '\0';
        /* Les línies mal formades o amb un camí relatiu es descarten */
        if (sscanf(line, "%llu %llu %n", &y, &x, &off) != 2 || off == 0 || line[off] != '/')
            continue;
        if (history_find(h, line + off) >= 0)
            continue;
        h->items[h->n].path = xstrdup(line + off);
        h->items[h->n].line = (size_t)y;
        h->items[h->n].col = (size_t)x;
        h->n++;
    }
    free(line);
    fclose(f);
}

int history_save(const History *h)
{
    char path[1100], tmp[1110], dir[1024];
    FILE *f;
    int err = 0;

    config_dir(dir, sizeof dir);
    mkdir(dir, 0700);   /* si ja existeix, res */
    history_path(path, sizeof path);
    snprintf(tmp, sizeof tmp, "%s.nou", path);
    if (!(f = fopen(tmp, "w")))
        return -1;
    for (size_t i = 0; i < h->n; i++)
        fprintf(f, "%zu %zu %s\n", h->items[i].line, h->items[i].col, h->items[i].path);
    if (fflush(f) != 0 || fsync(fileno(f)) != 0)
        err = errno;
    if (fclose(f) != 0 && !err)
        err = errno;
    if (!err && rename(tmp, path) != 0)
        err = errno;
    if (err) {
        unlink(tmp);
        errno = err;
        return -1;
    }
    return 0;
}

void history_free(History *h)
{
    for (size_t i = 0; i < h->n; i++)
        free(h->items[i].path);
    free(h->items);
    memset(h, 0, sizeof *h);
}

long history_find(const History *h, const char *abspath)
{
    for (size_t i = 0; i < h->n; i++)
        if (!strcmp(h->items[i].path, abspath))
            return (long)i;
    return -1;
}

void history_remove(History *h, size_t i)
{
    if (i >= h->n)
        return;
    free(h->items[i].path);
    memmove(&h->items[i], &h->items[i + 1], (h->n - i - 1) * sizeof *h->items);
    h->n--;
}

size_t history_prune_missing(History *h)
{
    size_t gone = 0;
    struct stat st;

    for (size_t i = h->n; i-- > 0;)
        if (stat(h->items[i].path, &st) != 0 || !S_ISREG(st.st_mode)) {
            history_remove(h, i);
            gone++;
        }
    return gone;
}

void history_touch(History *h, const char *abspath, size_t line, size_t col)
{
    long i = history_find(h, abspath);
    char *p;

    if (i >= 0) {
        p = h->items[i].path;
        memmove(&h->items[1], &h->items[0], (size_t)i * sizeof *h->items);
    } else {
        if (!h->items)
            h->items = xmalloc(HISTORY_MAX * sizeof *h->items);
        if (h->n == HISTORY_MAX)
            free(h->items[--h->n].path);
        memmove(&h->items[1], &h->items[0], h->n * sizeof *h->items);
        h->n++;
        p = xstrdup(abspath);
    }
    h->items[0].path = p;
    h->items[0].line = line;
    h->items[0].col = col;
}

int history_remember(const Editor *e)
{
    char abs[PATH_MAX];
    History h;
    int r;

    if (!e->filename || !realpath(e->filename, abs))
        return 0;   /* sense nom, o un fitxer nou que no s'ha desat */
    history_load(&h);
    history_touch(&h, abs, e->cy, e->cx);
    r = history_save(&h);
    history_free(&h);
    return r;
}

int history_restore(Editor *e)
{
    char abs[PATH_MAX];
    History h;
    long i;
    size_t y, x;
    const Line *l;

    if (!e->remember_pos || !e->filename || !realpath(e->filename, abs))
        return 0;
    history_load(&h);
    i = history_find(&h, abs);
    if (i < 0) {
        history_free(&h);
        return 0;
    }
    y = h.items[i].line;
    x = h.items[i].col;
    history_free(&h);
    /* El fitxer pot haver canviat fora de Phileas */
    if (y >= e->buf.nlines) {
        y = e->buf.nlines - 1;
        x = (size_t)-1;
    }
    l = &e->buf.lines[y];
    if (x > l->len)
        x = l->len;
    while (x > 0 && x < l->len && ((unsigned char)l->text[x] & 0xC0) == 0x80)
        x--;
    e->cy = y;
    e->cx = x;
    e->want_x = -1;
    e->top_line = y;
    e->top_row = 0;
    e->top_blank = 0;
    e->recenter = 1;
    return 1;
}
