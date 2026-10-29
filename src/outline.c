#include "outline.h"
#include "md.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>

static int is_blank(char c)
{
    return c == ' ' || c == '\t';
}

/* Títol d'una capçalera "## Títol ##": sense els "#" ni els espais. */
static char *atx_title(const char *s, size_t len, int *level)
{
    size_t i = 0, e = len, k;
    char *t;

    while (i < len && is_blank(s[i]))
        i++;
    for (k = i; k < len && s[k] == '#'; k++)
        ;
    *level = (int)(k - i);
    i = k;
    while (i < len && is_blank(s[i]))
        i++;
    while (e > i && is_blank(s[e - 1]))
        e--;
    /* Tanca opcional: només si va precedida d'un espai (o és tot el títol) */
    k = e;
    while (k > i && s[k - 1] == '#')
        k--;
    if (k < e && (k == i || is_blank(s[k - 1]))) {
        e = k;
        while (e > i && is_blank(s[e - 1]))
            e--;
    }
    t = xmalloc(e - i + 1);
    memcpy(t, s + i, e - i);
    t[e - i] = '\0';
    return t;
}

static char *plain_title(const char *s, size_t len)
{
    size_t i = 0, e = len;
    char *t;

    while (i < len && is_blank(s[i]))
        i++;
    while (e > i && is_blank(s[e - 1]))
        e--;
    t = xmalloc(e - i + 1);
    memcpy(t, s + i, e - i);
    t[e - i] = '\0';
    return t;
}

static void add(Outline *o, size_t *cap, size_t line, int level, char *title)
{
    if (o->n == *cap) {
        *cap = *cap ? *cap * 2 : 16;
        o->items = xrealloc(o->items, *cap * sizeof *o->items);
    }
    o->items[o->n++] = (OutlineItem){ line, level, title, 0 };
}

void outline_build(Buffer *b, Outline *o)
{
    size_t cap = 0, *own;

    memset(o, 0, sizeof *o);
    if (b->plain)
        return;
    buf_scan(b);
    for (size_t i = 0; i < b->nlines; i++) {
        Line *l = &b->lines[i];
        int level;

        if (MD_BASE(l->kind) == MD_HEADING) {
            char *t = atx_title(l->text, l->len, &level);
            add(o, &cap, i, level, t);
        } else if (MD_BASE(l->kind) == MD_TEXT && i + 1 < b->nlines &&
                   MD_BASE(b->lines[i + 1].kind) == MD_UNDERLINE) {
            add(o, &cap, i, strchr(b->lines[i + 1].text, '=') ? 1 : 2,
                plain_title(l->text, l->len));
        }
    }

    /* Paraules de cada secció fins a la capçalera següent, i després hi
     * sumem les de les subseccions */
    own = xmalloc((o->n + 1) * sizeof *own);
    for (size_t k = 0, i = 0; k <= o->n; k++) {
        size_t end = k < o->n ? o->items[k].line : b->nlines, n = 0;

        for (; i < end; i++)
            n += buf_line_words(b, i);
        if (k == 0)
            o->first_words = n;
        else
            own[k - 1] = n;
        i = end + 1;   /* la capçalera no compta */
    }
    for (size_t k = 0; k < o->n; k++) {
        o->items[k].words = own[k];
        for (size_t j = k + 1; j < o->n && o->items[j].level > o->items[k].level; j++)
            o->items[k].words += own[j];
    }
    free(own);
}

void outline_free(Outline *o)
{
    for (size_t k = 0; k < o->n; k++)
        free(o->items[k].title);
    free(o->items);
    memset(o, 0, sizeof *o);
}

size_t outline_at(const Outline *o, size_t line)
{
    size_t lo = 0, hi = o->n;

    /* Cerca binària: la primera capçalera posterior a line */
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;

        if (o->items[mid].line <= line)
            lo = mid + 1;
        else
            hi = mid;
    }
    return lo ? lo - 1 : OUTLINE_NONE;
}

size_t outline_end(const Outline *o, size_t k, size_t nlines)
{
    for (size_t j = k + 1; j < o->n; j++)
        if (o->items[j].level <= o->items[k].level)
            return o->items[j].line;
    return nlines;
}

int outline_move(const Outline *o, size_t k, int dir, size_t nlines,
                 size_t *a, size_t *b, size_t *c, size_t *head)
{
    int level = o->items[k].level;
    size_t j;

    if (dir < 0) {
        /* L'anterior del mateix nivell, sense sortir de la secció pare */
        for (j = k; j > 0 && o->items[j - 1].level > level; j--)
            ;
        if (j == 0 || o->items[j - 1].level != level)
            return 0;
        *a = o->items[j - 1].line;
        *b = o->items[k].line;
        *c = outline_end(o, k, nlines);
        *head = *a;
        return 1;
    }
    *b = outline_end(o, k, nlines);
    for (j = k + 1; j < o->n && o->items[j].line != *b; j++)
        ;
    if (j == o->n || o->items[j].level != level)
        return 0;
    *a = o->items[k].line;
    *c = outline_end(o, j, nlines);
    *head = *a + (*c - *b);
    return 1;
}
