#include "flat.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>

static void add(Flat *f, FNode *n, int depth)
{
    if (f->n == f->cap)
        f->rows = xrealloc(f->rows, (f->cap = f->cap ? f->cap * 2 : 64) * sizeof *f->rows);
    f->rows[f->n].node = n;
    f->rows[f->n].depth = depth;
    f->n++;
}

static int self_match(const FNode *n, const char *filter)
{
    return fk_match(n->title, filter) || (n->sinopsi && n->sinopsi[0] && fk_match(n->sinopsi, filter));
}

/* El node o algun descendent compleix el filtre */
static int included(const FNode *n, const char *filter)
{
    if (n->kind != FK_GROUP && self_match(n, filter))
        return 1;
    for (size_t i = 0; i < n->nkids; i++)
        if (included(n->kids[i], filter))
            return 1;
    return 0;
}

static void walk(Flat *f, FNode *n, int depth, const char *filter)
{
    add(f, n, depth);
    for (size_t i = 0; i < n->nkids; i++) {
        FNode *k = n->kids[i];

        if (filter ? included(k, filter) : n->open)
            walk(f, k, depth + 1, filter);
    }
}

void flat_build(Flat *f, const Project *p, const char *filter)
{
    memset(f, 0, sizeof *f);
    if (filter && !*filter)
        filter = NULL;
    if (!p->top)
        return;
    for (size_t i = 0; i < p->top->nkids; i++) {
        FNode *g = p->top->kids[i];

        if (!filter || included(g, filter))
            walk(f, g, 0, filter);
    }
}

void flat_free(Flat *f)
{
    free(f->rows);
    memset(f, 0, sizeof *f);
}

long flat_find(const Flat *f, const FNode *n)
{
    for (size_t i = 0; i < f->n; i++)
        if (f->rows[i].node == n)
            return (long)i;
    return -1;
}
