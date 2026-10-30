#include "footnote.h"
#include "md.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NONE ((size_t)-1)

int fn_line_ok(const Buffer *b, size_t i)
{
    if (b->plain || i >= b->nlines)
        return 0;
    switch (MD_BASE(b->lines[i].kind)) {
    case MD_TEXT:
    case MD_HEADING:
    case MD_LIST:
    case MD_QUOTE:
    case MD_TABLE:
    case MD_INDENT:
        return 1;
    }
    return 0;
}

/* Si la línia i és una definició, l'etiqueta (a *la, *ln) i on comencen
 * les crides del text de la nota. Si no, NONE. */
static size_t def_label(const Buffer *b, size_t i, const char **la, size_t *ln)
{
    const Line *l = &b->lines[i];
    size_t d, e;

    if (!fn_line_ok(b, i) || (d = md_footnote_def(l->text, l->len)) == NONE)
        return NONE;
    e = md_footnote(l->text, l->len, d);
    if (la) {
        *la = l->text + d + 2;
        *ln = e - 1 - (d + 2);
    }
    return e + 1;
}

/* Crida següent de la línia i a partir de *from: el tram [*a, *z) de la
 * crida sencera. *from ha de començar a ref_start(). */
static int next_ref(const Buffer *b, size_t i, size_t *from, size_t *a, size_t *z)
{
    const Line *l = &b->lines[i];
    size_t j;

    if (*from >= l->len || (j = md_next_footnote(l->text, l->len, *from)) == NONE)
        return 0;
    *a = j;
    *z = *from = md_footnote(l->text, l->len, j);
    return 1;
}

static size_t ref_start(const Buffer *b, size_t i)
{
    size_t s = def_label(b, i, NULL, NULL);

    return s == NONE ? 0 : s;
}

static int same(const char *s, size_t n, const char *t, size_t m)
{
    return n == m && !memcmp(s, t, n);
}

static int numeric(const char *s, size_t n)
{
    if (!n)
        return 0;
    for (size_t i = 0; i < n; i++)
        if (s[i] < '0' || s[i] > '9')
            return 0;
    return 1;
}

static void consider(const char *s, size_t n, unsigned long *max)
{
    char tmp[16];
    unsigned long v;

    if (!numeric(s, n) || n >= 10)
        return;
    memcpy(tmp, s, n);
    tmp[n] = '\0';
    v = strtoul(tmp, NULL, 10);
    if (v > *max)
        *max = v;
}

unsigned long fn_next_number(const Buffer *b)
{
    unsigned long max = 0;
    const char *la;
    size_t ln, from, a, z;

    for (size_t i = 0; i < b->nlines; i++) {
        if (!fn_line_ok(b, i))
            continue;
        if (def_label(b, i, &la, &ln) != NONE)
            consider(la, ln, &max);
        for (from = ref_start(b, i); next_ref(b, i, &from, &a, &z);)
            consider(b->lines[i].text + a + 2, z - a - 3, &max);
    }
    return max + 1;
}

int fn_in_def(const Buffer *b, size_t i, size_t *def)
{
    size_t j = i;

    while (j > 0 && fn_line_ok(b, j) && MD_BASE(b->lines[j].kind) == MD_INDENT)
        j--;
    if (def_label(b, j, NULL, NULL) == NONE)
        return 0;
    *def = j;
    return 1;
}

int fn_ref_near(const Buffer *b, Pos pos, size_t *ra, size_t *rz)
{
    size_t from, a, z, best = NONE, dist;
    int found = 0;

    if (!fn_line_ok(b, pos.line))
        return 0;
    for (from = ref_start(b, pos.line); next_ref(b, pos.line, &from, &a, &z);) {
        if (pos.col >= a && pos.col <= z)
            dist = 0;
        else
            dist = pos.col < a ? a - pos.col : pos.col - z;
        if (dist < best) {
            best = dist;
            *ra = a;
            *rz = z;
            found = 1;
        }
    }
    return found;
}

int fn_find_def(const Buffer *b, const char *label, size_t n, size_t *line)
{
    const char *la;
    size_t ln;

    for (size_t i = 0; i < b->nlines; i++)
        if (def_label(b, i, &la, &ln) != NONE && same(la, ln, label, n)) {
            *line = i;
            return 1;
        }
    return 0;
}

int fn_find_ref(const Buffer *b, const char *label, size_t n, Pos *after)
{
    size_t from, a, z;

    for (size_t i = 0; i < b->nlines; i++) {
        if (!fn_line_ok(b, i))
            continue;
        for (from = ref_start(b, i); next_ref(b, i, &from, &a, &z);)
            if (same(b->lines[i].text + a + 2, z - a - 3, label, n)) {
                *after = (Pos){ i, z };
                return 1;
            }
    }
    return 0;
}

Pos fn_insert(Buffer *b, Pos at, unsigned long n)
{
    char call[32], def[40];
    int nc = snprintf(call, sizeof call, "[^%lu]", n);
    int nd = snprintf(def, sizeof def, "\n[^%lu]: ", n);
    size_t last;
    const char *d = def;

    buf_insert(b, at, call, (size_t)nc);
    /* La definició, al final: en un paràgraf nou, o a l'últim si és buit */
    last = b->nlines - 1;
    if (b->lines[last].len == 0)
        d++, nd--;
    return buf_insert(b, (Pos){ last, b->lines[last].len }, d, (size_t)nd);
}

/* ---- Renumerar ---- */

typedef struct {
    char  *name;      /* etiqueta original */
    size_t len;
    char  *rename;    /* la nova (la mateixa, si té nom) */
} Label;

typedef struct {
    size_t line, a, z;   /* tram de l'etiqueta, sense "[^" ni "]" */
    size_t label;
} Occ;

typedef struct {
    size_t start, end;   /* línies [start, end) */
    size_t label;
} Block;

static size_t find_label(const Label *t, size_t n, const char *s, size_t len)
{
    for (size_t k = 0; k < n; k++)
        if (same(t[k].name, t[k].len, s, len))
            return k;
    return NONE;
}

static size_t add_label(Label **t, size_t *n, size_t *cap, const char *s, size_t len)
{
    size_t k = find_label(*t, *n, s, len);

    if (k != NONE)
        return k;
    if (*n == *cap) {
        *cap = *cap ? *cap * 2 : 16;
        *t = xrealloc(*t, *cap * sizeof **t);
    }
    (*t)[*n].name = xmalloc(len + 1);
    memcpy((*t)[*n].name, s, len);
    (*t)[*n].name[len] = '\0';
    (*t)[*n].len = len;
    (*t)[*n].rename = NULL;
    return (*n)++;
}

static void push_occ(Occ **o, size_t *n, size_t *cap, Occ v)
{
    if (*n == *cap) {
        *cap = *cap ? *cap * 2 : 32;
        *o = xrealloc(*o, *cap * sizeof **o);
    }
    (*o)[(*n)++] = v;
}

static void append(char **s, size_t *n, size_t *cap, const char *t, size_t m)
{
    if (*n + m + 1 > *cap) {
        *cap = (*n + m + 1) * 2;
        *s = xrealloc(*s, *cap);
    }
    memcpy(*s + *n, t, m);
    *n += m;
}

/* Ordena les definicions segons l'ordre de les etiquetes, als llocs on ja
 * eren: la primera posició rep la primera nota, i així successivament. */
static int sort_defs(Buffer *b, const Label *t, size_t nt)
{
    Block *bl = NULL, *sorted;
    size_t nb = 0, cap = 0, k, i, first, end, n = 0, tcap = 0;
    const char *la;
    size_t ln;
    char *text = NULL;
    int ordered = 1;

    for (i = 0; i < b->nlines; i++) {
        if (def_label(b, i, &la, &ln) == NONE)
            continue;
        if (nb == cap) {
            cap = cap ? cap * 2 : 16;
            bl = xrealloc(bl, cap * sizeof *bl);
        }
        bl[nb].start = i;
        for (end = i + 1; end < b->nlines && fn_line_ok(b, end) &&
             MD_BASE(b->lines[end].kind) == MD_INDENT; end++)
            ;
        bl[nb].end = end;
        bl[nb].label = NONE;
        for (k = 0; k < nt; k++)
            if (same(t[k].rename, strlen(t[k].rename), la, ln)) {
                bl[nb].label = k;
                break;
            }
        if (nb && bl[nb].label < bl[nb - 1].label)
            ordered = 0;
        nb++;
        i = end - 1;
    }
    if (ordered) {
        free(bl);
        return 0;
    }

    /* Ordenació estable per inserció (hi ha poques notes) */
    sorted = xmalloc(nb * sizeof *sorted);
    for (k = 0; k < nb; k++) {
        size_t j = k;

        while (j > 0 && sorted[j - 1].label > bl[k].label) {
            sorted[j] = sorted[j - 1];
            j--;
        }
        sorted[j] = bl[k];
    }

    first = bl[0].start;
    end = bl[nb - 1].end;
    for (i = first, k = 0; i < end;) {
        size_t from = i, to = i + 1;

        if (k < nb && i == bl[k].start) {
            from = sorted[k].start;
            to = sorted[k].end;
            i = bl[k].end;
            k++;
        } else {
            i++;
        }
        for (size_t j = from; j < to; j++) {
            if (n)
                append(&text, &n, &tcap, "\n", 1);
            append(&text, &n, &tcap, b->lines[j].text, b->lines[j].len);
        }
    }
    buf_delete(b, (Pos){ first, 0 }, (Pos){ end - 1, b->lines[end - 1].len });
    buf_insert(b, (Pos){ first, 0 }, text ? text : "", n);
    free(text);
    free(sorted);
    free(bl);
    return 1;
}

int fn_renumber(Buffer *b, size_t *notes)
{
    Label *t = NULL;
    Occ *o = NULL;
    size_t nt = 0, tcap = 0, no = 0, ocap = 0, i, k, from, a, z, ln;
    unsigned long next = 1;
    const char *la;
    int changed = 0;

    /* Les etiquetes, en l'ordre de la primera crida */
    for (i = 0; i < b->nlines; i++) {
        if (!fn_line_ok(b, i))
            continue;
        for (from = ref_start(b, i); next_ref(b, i, &from, &a, &z);)
            add_label(&t, &nt, &tcap, b->lines[i].text + a + 2, z - a - 3);
    }
    /* Totes les aparicions, i les definicions sense crida al final */
    for (i = 0; i < b->nlines; i++) {
        if (!fn_line_ok(b, i))
            continue;
        if ((from = def_label(b, i, &la, &ln)) != NONE) {
            a = (size_t)(la - b->lines[i].text);
            push_occ(&o, &no, &ocap, (Occ){ i, a, a + ln, add_label(&t, &nt, &tcap, la, ln) });
        } else {
            from = 0;
        }
        while (next_ref(b, i, &from, &a, &z))
            push_occ(&o, &no, &ocap, (Occ){ i, a + 2, z - 1,
                     find_label(t, nt, b->lines[i].text + a + 2, z - a - 3) });
    }
    for (k = 0; k < nt; k++) {
        char num[24];

        if (numeric(t[k].name, t[k].len)) {
            snprintf(num, sizeof num, "%lu", next++);
            t[k].rename = xstrdup(num);
        } else {
            t[k].rename = xstrdup(t[k].name);
        }
    }

    /* Del final cap al principi: les posicions anteriors no es mouen */
    for (k = no; k-- > 0;) {
        const Label *l = &t[o[k].label];

        if (!strcmp(l->name, l->rename))
            continue;
        buf_delete(b, (Pos){ o[k].line, o[k].a }, (Pos){ o[k].line, o[k].z });
        buf_insert(b, (Pos){ o[k].line, o[k].a }, l->rename, strlen(l->rename));
        changed = 1;
    }
    buf_scan(b);
    if (nt && sort_defs(b, t, nt))
        changed = 1;

    *notes = nt;
    for (k = 0; k < nt; k++) {
        free(t[k].name);
        free(t[k].rename);
    }
    free(t);
    free(o);
    return changed;
}
