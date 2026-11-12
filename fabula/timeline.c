#include "timeline.h"
#include "diary.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int dim(int y, int m)
{
    static const int d[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

    if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0))
        return 29;
    return d[m - 1];
}

int timeline_range(const char *s, long *lo, long *hi)
{
    int y, m, d, n = 0;

    if (!s || !*s)
        return -1;
    if (sscanf(s, "%4d%n", &y, &n) == 1 && n == 4 && !s[4]) {
        if (y < 1)
            return -1;
        *lo = diary_civil_days(y, 1, 1);
        *hi = diary_civil_days(y, 12, 31);
        return 0;
    }
    if (sscanf(s, "%4d-%2d%n", &y, &m, &n) == 2 && n == 7 && !s[7] && y >= 1 && m >= 1 && m <= 12) {
        *lo = diary_civil_days(y, m, 1);
        *hi = diary_civil_days(y, m, dim(y, m));
        return 0;
    }
    if (sscanf(s, "%4d-%2d-%2d%n", &y, &m, &d, &n) == 3 && n == 10 && !s[10] && y >= 1 && m >= 1 &&
        m <= 12 && d >= 1 && d <= dim(y, m)) {
        *lo = *hi = diary_civil_days(y, m, d);
        return 0;
    }
    return -1;
}

static int cmp(const void *a, const void *b)
{
    const TLItem *x = a, *y = b;

    if (x->dated != y->dated)
        return x->dated ? -1 : 1;
    if (x->lo != y->lo)
        return x->lo < y->lo ? -1 : 1;
    if (x->hi != y->hi)
        return x->hi < y->hi ? -1 : 1;
    return x->seq < y->seq ? -1 : x->seq > y->seq;
}

static int add(Timeline *t, size_t *cap, FNode *n, size_t seq)
{
    TLItem *it;

    if (t->n == *cap) {
        TLItem *nt = realloc(t->items, (*cap ? *cap * 2 : 32) * sizeof *nt);

        if (!nt)
            return -1;
        t->items = nt;
        *cap = *cap ? *cap * 2 : 32;
    }
    it = &t->items[t->n++];
    memset(it, 0, sizeof *it);
    it->node = n;
    it->seq = seq;
    if (n->data && *n->data) {
        if (timeline_range(n->data, &it->lo, &it->hi) == 0) {
            it->dated = 1;
        } else {
            it->bad = 1;
            t->bad++;
        }
    }
    return 0;
}

int timeline_build(const Project *p, Timeline *t)
{
    size_t cap = 0, seq = 0, i;
    long maxlo = 0;
    int have = 0;

    memset(t, 0, sizeof *t);
    if (!p->manuscrit)
        return 0;
    for (i = 0; i < p->manuscrit->nkids; i++) {
        FNode *c = p->manuscrit->kids[i];

        if (!c->nkids) {
            if (add(t, &cap, c, seq++) != 0)
                goto fail;
        } else {
            for (size_t k = 0; k < c->nkids; k++)
                if (add(t, &cap, c->kids[k], seq++) != 0)
                    goto fail;
        }
    }
    /* salts enrere, en l'ordre del text */
    for (i = 0; i < t->n; i++) {
        TLItem *it = &t->items[i];

        if (!it->dated)
            continue;
        if (have && it->hi < maxlo) {
            it->back = 1;
            t->backs++;
        }
        if (!have || it->lo > maxlo)
            maxlo = it->lo;
        have = 1;
    }
    qsort(t->items, t->n, sizeof *t->items, cmp);
    for (i = 0; i < t->n; i++) {
        if (t->items[i].dated)
            t->dated++;
        else
            t->undated++;
    }
    return 0;
fail:
    timeline_free(t);
    return -1;
}

void timeline_free(Timeline *t)
{
    free(t->items);
    memset(t, 0, sizeof *t);
}
