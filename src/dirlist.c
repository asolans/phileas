#include "dirlist.h"
#include "utf8.h"
#include "util.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <wctype.h>

static int compare(const void *a, const void *b)
{
    const DirEntry *x = a, *y = b;

    if (x->is_dir != y->is_dir)
        return y->is_dir - x->is_dir;
    return strcoll(x->name, y->name);
}

int dirlist_read(DirList *l, const char *dir, int hidden)
{
    DIR *d = opendir(dir);
    struct dirent *de;
    char path[4096];
    int err;

    dirlist_free(l);
    if (!d)
        return -1;
    while ((de = readdir(d))) {
        struct stat st;
        DirEntry *it;

        if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, ".."))
            continue;
        if (!hidden && de->d_name[0] == '.')
            continue;
        path_join(dir, de->d_name, path, sizeof path);
        /* stat segueix els enllaços; si l'enllaç està trencat, es mostra com a fitxer */
        if (stat(path, &st) < 0 && lstat(path, &st) < 0)
            continue;
        if (l->n == l->cap) {
            l->cap = l->cap ? l->cap * 2 : 64;
            l->items = xrealloc(l->items, l->cap * sizeof *l->items);
        }
        it = &l->items[l->n++];
        it->name = xstrdup(de->d_name);
        it->is_dir = S_ISDIR(st.st_mode);
        it->size = st.st_size;
        it->mtime = st.st_mtime;
    }
    err = errno;
    closedir(d);
    qsort(l->items, l->n, sizeof *l->items, compare);
    errno = err;
    return 0;
}

void dirlist_free(DirList *l)
{
    for (size_t i = 0; i < l->n; i++)
        free(l->items[i].name);
    free(l->items);
    memset(l, 0, sizeof *l);
}

/* Caràcter següent de s en minúscula, o 0 al final. */
static wchar_t lower_at(const char *s, size_t len, size_t *i)
{
    wchar_t wc;

    if (*i >= len)
        return 0;
    *i += utf8_decode(s, len, *i, &wc);
    return wc == UTF8_INVALID ? wc : (wchar_t)towlower((wint_t)wc);
}

int dirlist_match(const char *name, const char *filter)
{
    size_t nlen = strlen(name), flen = strlen(filter);

    for (size_t start = 0; start <= nlen; start = utf8_next(name, nlen, start)) {
        size_t i = start, j = 0;
        wchar_t a, b;

        for (;;) {
            b = lower_at(filter, flen, &j);
            if (!b)
                return 1;
            a = lower_at(name, nlen, &i);
            if (a != b)
                break;
        }
        if (start == nlen)
            break;
    }
    return 0;
}

void path_parent(const char *path, char *out, size_t outsz)
{
    const char *slash = strrchr(path, '/');
    size_t n = slash ? (size_t)(slash - path) : 0;

    if (n == 0)
        snprintf(out, outsz, "/");
    else
        snprintf(out, outsz, "%.*s", (int)n, path);
}

void path_join(const char *dir, const char *name, char *out, size_t outsz)
{
    size_t n = strlen(dir);

    snprintf(out, outsz, "%s%s%s", dir, n > 0 && dir[n - 1] == '/' ? "" : "/", name);
}

void format_size(off_t size, char *out, size_t outsz)
{
    static const char *units[] = { "KB", "MB", "GB", "TB" };
    double v = (double)size;
    int u = -1;

    if (size < 1024) {
        snprintf(out, outsz, "%ld B", (long)size);
        return;
    }
    while (v >= 1024 && u < 3) {
        v /= 1024;
        u++;
    }
    if (v < 10)
        snprintf(out, outsz, "%d,%d %s", (int)v, (int)(v * 10) % 10, units[u]);
    else
        snprintf(out, outsz, "%.0f %s", v, units[u]);
}
