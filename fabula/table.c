#include "table.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void add(Table *t, FNode *n, int depth)
{
    if (t->n == t->cap)
        t->rows = xrealloc(t->rows, (t->cap = t->cap ? t->cap * 2 : 64) * sizeof *t->rows);
    t->rows[t->n].node = n;
    t->rows[t->n].depth = depth;
    t->rows[t->n].seq = t->n;
    t->n++;
}

static int matches(const FNode *n, const char *filter)
{
    return !filter || fk_node_match(n, filter);
}

static const char *raw(const FNode *n, int col)
{
    switch (col) {
    case TC_TITLE: return n->title;
    case TC_POV:   return n->pov;
    case TC_LLOC:  return n->lloc;
    case TC_DATA:  return n->data;
    case TC_ESTAT: return n->estat ? n->estat : "";
    default:       return "";
    }
}

static int g_col, g_desc;

static int cmp_rows(const void *a, const void *b)
{
    const TRow *x = a, *y = b;
    int r;

    if (g_col == TC_WORDS) {
        r = x->node->words < y->node->words ? -1 : x->node->words > y->node->words;
        if (g_desc)
            r = -r;
    } else {
        const char *sx = raw(x->node, g_col), *sy = raw(y->node, g_col);

        if (!*sx || !*sy) {                       /* els buits, sempre al final */
            if (!*sx && !*sy)
                r = 0;
            else
                return *sx ? -1 : 1;
        } else if (g_col == TC_DATA) {
            r = strcmp(sx, sy);
        } else {
            r = fk_name_cmp(sx, sy);
        }
        if (g_desc)
            r = -r;
    }
    if (r)
        return r;
    return x->seq < y->seq ? -1 : 1;
}

void table_build(Table *t, const Project *p, const char *filter, int sort_col, int desc)
{
    memset(t, 0, sizeof *t);
    if (filter && !*filter)
        filter = NULL;
    if (!p->manuscrit)
        return;
    t->flat = sort_col >= 0 && sort_col < TC_N;
    for (size_t i = 0; i < p->manuscrit->nkids; i++) {
        FNode *c = p->manuscrit->kids[i];
        int any = 0;

        if (t->flat) {
            if (!c->nkids) {
                if (matches(c, filter))
                    add(t, c, 0);
                continue;
            }
            for (size_t k = 0; k < c->nkids; k++)
                if (matches(c->kids[k], filter))
                    add(t, c->kids[k], 0);
            continue;
        }
        for (size_t k = 0; k < c->nkids; k++)
            any |= matches(c->kids[k], filter);
        if (!matches(c, filter) && !any)
            continue;
        add(t, c, 0);
        for (size_t k = 0; k < c->nkids; k++)
            if (matches(c->kids[k], filter))
                add(t, c->kids[k], 1);
    }
    if (t->flat && t->n > 1) {
        g_col = sort_col;
        g_desc = desc;
        qsort(t->rows, t->n, sizeof *t->rows, cmp_rows);
    }
}

void table_free(Table *t)
{
    free(t->rows);
    memset(t, 0, sizeof *t);
}

long table_find(const Table *t, const FNode *n)
{
    for (size_t i = 0; i < t->n; i++)
        if (t->rows[i].node == n)
            return (long)i;
    return -1;
}

const char *table_cell(const Table *t, const FNode *n, int col, char *buf, size_t bsz)
{
    if (col == TC_WORDS)
        fk_fmt_num(n->words, buf, bsz);
    else if (col == TC_TITLE && t->flat && n->kind == FK_SCENE && n->parent)
        snprintf(buf, bsz, "%s › %s", n->parent->title, n->title);
    else
        snprintf(buf, bsz, "%s", raw(n, col));
    return buf;
}

const char *table_col_name(int col)
{
    static const char *const names[TC_N] = { "Títol", "POV", "Lloc", "Data", "Estat", "Paraules" };

    return col >= 0 && col < TC_N ? names[col] : "";
}

const char *table_col_key(int col)
{
    static const char *const keys[TC_N] = { NULL, "pov", "lloc", "data", "estat", NULL };

    return col >= 0 && col < TC_N ? keys[col] : NULL;
}
