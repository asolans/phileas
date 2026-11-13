#include "chooser.h"

#include "ui.h"
#include "util.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int cmp_str(const void *a, const void *b)
{
    return strcoll(*(char *const *)a, *(char *const *)b);
}

size_t cm_dirs(const char *dir, const char *need, char ***out)
{
    DIR *d = opendir(dir);
    struct dirent *de;
    char **v = NULL, path[4096];
    size_t n = 0, cap = 0;
    struct stat st;

    *out = NULL;
    if (!d)
        return 0;
    while ((de = readdir(d))) {
        if (de->d_name[0] == '.')
            continue;
        snprintf(path, sizeof path, "%s/%s", dir, de->d_name);
        if (stat(path, &st) != 0 || !S_ISDIR(st.st_mode))
            continue;
        if (need) {
            snprintf(path, sizeof path, "%s/%s/%s", dir, de->d_name, need);
            if (access(path, F_OK) != 0)
                continue;
        }
        if (n == cap)
            v = xrealloc(v, (cap = cap ? 2 * cap : 16) * sizeof *v);
        v[n++] = xstrdup(de->d_name);
    }
    closedir(d);
    if (n)
        qsort(v, n, sizeof *v, cmp_str);
    *out = v;
    return n;
}

void cm_dirs_free(char **v, size_t n)
{
    for (size_t i = 0; i < n; i++)
        free(v[i]);
    free(v);
}

int cm_choose(const char *title, const char *const *items, size_t n, const char *extra,
              const char *empty)
{
    size_t total = n + (extra ? 1 : 0), sel = 0, top = 0;

    for (;;) {
        int rows = LINES - 4, key, y0 = 2;
        char head[512];

        if (rows < 1)
            rows = 1;
        if (sel < top)
            top = sel;
        if (sel >= top + (size_t)rows)
            top = sel - (size_t)rows + 1;
        erase();
        snprintf(head, sizeof head, " %s", title);
        cm_blank(0, 0, COLS, ft_attr(FT_BAR));
        cm_put(0, 0, head, COLS, ft_attr(FT_BAR));
        if (!n && empty) {
            cm_put(y0, 3, empty, COLS - 4, ft_attr(FT_DIM));
            y0 += 2;
            rows -= 2;
        }
        for (int r = 0; r < rows && top + (size_t)r < total; r++) {
            size_t i = top + (size_t)r;
            attr_t a = i == sel ? ft_attr(FT_SEL) : 0;
            const char *label = i < n ? items[i] : extra;

            cm_blank(y0 + r, 1, COLS - 2, a);
            cm_put(y0 + r, 3, label, COLS - 5, i < n ? a : (a ? a : ft_attr(FT_DIM)));
        }
        cm_blank(LINES - 1, 0, COLS, ft_attr(FT_BAR));
        cm_put(LINES - 1, 0, " ↑↓ tria · Retorn obre · Esc surt", COLS, ft_attr(FT_BAR));
        refresh();
        key = key_read();
        if (key < 0 || key == K_FN(KEY_RESIZE))
            continue;
        if (key == 27 || key == 'q' || key == 17)           /* Esc, q, Ctrl+Q */
            return -1;
        if (!total)
            continue;
        if (key == K_FN(KEY_UP))
            sel = sel ? sel - 1 : total - 1;
        else if (key == K_FN(KEY_DOWN))
            sel = sel + 1 < total ? sel + 1 : 0;
        else if (key == K_FN(KEY_HOME))
            sel = 0;
        else if (key == K_FN(KEY_END))
            sel = total - 1;
        else if (key == K_FN(KEY_PPAGE))
            sel = sel > (size_t)rows ? sel - (size_t)rows : 0;
        else if (key == K_FN(KEY_NPAGE))
            sel = sel + (size_t)rows < total ? sel + (size_t)rows : total - 1;
        else if (key == '\n')
            return (int)sel;
    }
}
