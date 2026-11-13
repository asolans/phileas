#include "model.h"

#include "fold.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>

static char *col(sqlite3_stmt *s, int i)
{
    const char *t = (const char *)sqlite3_column_text(s, i);

    return xstrdup(t ? t : "");
}

static int tag_cmp(const void *a, const void *b)
{
    const MTag *x = a, *y = b;

    return x->fitxa < y->fitxa ? -1 : x->fitxa > y->fitxa;
}

static void nav_free(Model *m)
{
    for (size_t i = 0; i < m->nnav; i++) {
        free(m->nav[i].label);
        free(m->nav[i].key);
    }
    free(m->nav);
    m->nav = NULL;
    m->nnav = 0;
}

void model_free(Model *m)
{
    for (size_t i = 0; i < m->n; i++) {
        MNote *n = &m->notes[i];

        free(n->cami);
        free(n->carpeta);
        free(n->nom);
        free(n->fold);
        free(n->tipus);
        free(n->data);
    }
    free(m->notes);
    for (size_t i = 0; i < m->ntags; i++)
        free(m->tags[i].tag);
    free(m->tags);
    nav_free(m);
    m->notes = NULL;
    m->tags = NULL;
    m->n = m->ntags = 0;
}

int model_load(Model *m, sqlite3 *db)
{
    sqlite3_stmt *s;
    size_t cap = 0;
    NavMode mode = m->mode;

    model_free(m);
    m->mode = mode;
    if (sqlite3_prepare_v2(db, "SELECT id, cami, carpeta, nom, tipus, data, paraules, mtime FROM fitxes ORDER BY id",
                           -1, &s, NULL) != SQLITE_OK)
        return -1;
    while (sqlite3_step(s) == SQLITE_ROW) {
        MNote *n;

        if (m->n == cap) {
            cap = cap ? 2 * cap : 512;
            m->notes = xrealloc(m->notes, cap * sizeof *m->notes);
        }
        n = &m->notes[m->n++];
        n->id = sqlite3_column_int64(s, 0);
        n->cami = col(s, 1);
        n->carpeta = col(s, 2);
        n->nom = col(s, 3);
        n->fold = cm_fold(n->nom, strlen(n->nom));
        n->tipus = col(s, 4);
        n->data = col(s, 5);
        n->paraules = sqlite3_column_int64(s, 6);
        n->mtime = sqlite3_column_int64(s, 7);
    }
    sqlite3_finalize(s);

    cap = 0;
    if (sqlite3_prepare_v2(db, "SELECT DISTINCT fitxa, etiqueta_min FROM etiquetes", -1, &s, NULL) != SQLITE_OK)
        return -1;
    while (sqlite3_step(s) == SQLITE_ROW) {
        if (m->ntags == cap) {
            cap = cap ? 2 * cap : 1024;
            m->tags = xrealloc(m->tags, cap * sizeof *m->tags);
        }
        m->tags[m->ntags].fitxa = sqlite3_column_int64(s, 0);
        m->tags[m->ntags].tag = col(s, 1);
        m->ntags++;
    }
    sqlite3_finalize(s);
    qsort(m->tags, m->ntags, sizeof *m->tags, tag_cmp);
    model_set_mode(m, m->mode);
    return 0;
}

long model_find(const Model *m, long long id)
{
    size_t lo = 0, hi = m->n;           /* les notes són per id */

    while (lo < hi) {
        size_t mid = (lo + hi) / 2;

        if (m->notes[mid].id < id)
            lo = mid + 1;
        else
            hi = mid;
    }
    return lo < m->n && m->notes[lo].id == id ? (long)lo : -1;
}

/* ---------- navegació ---------- */

typedef struct {
    char **v;
    size_t n, cap;
} Strs;

static void strs_add(Strs *s, const char *x)
{
    if (s->n == s->cap) {
        s->cap = s->cap ? 2 * s->cap : 64;
        s->v = xrealloc(s->v, s->cap * sizeof *s->v);
    }
    s->v[s->n++] = xstrdup(x);
}

static int str_cmp(const void *a, const void *b)
{
    return strcmp(*(char *const *)a, *(char *const *)b);
}

/* Ordena i treu repetits; hi afegeix tots els prefixos «a/b» de «a/b/c» */
static void strs_tree(Strs *s)
{
    size_t n0 = s->n;

    for (size_t i = 0; i < n0; i++)
        for (char *p = strchr(s->v[i], '/'); p; p = strchr(p + 1, '/')) {
            *p = '\0';
            strs_add(s, s->v[i]);
            *p = '/';
        }
    qsort(s->v, s->n, sizeof *s->v, str_cmp);
    {
        size_t o = 0;

        for (size_t i = 0; i < s->n; i++) {
            if (o && !strcmp(s->v[o - 1], s->v[i]))
                free(s->v[i]);
            else
                s->v[o++] = s->v[i];
        }
        s->n = o;
    }
}

/* x és key o és dins de key («key/…») */
static int under(const char *x, const char *key)
{
    size_t k = strlen(key);

    return !strncmp(x, key, k) && (x[k] == '\0' || x[k] == '/');
}

static void nav_add(Model *m, size_t *cap, const char *label, const char *key, int depth, size_t count)
{
    if (m->nnav == *cap) {
        *cap = *cap ? 2 * *cap : 32;
        m->nav = xrealloc(m->nav, *cap * sizeof *m->nav);
    }
    m->nav[m->nnav].label = xstrdup(label);
    m->nav[m->nnav].key = key ? xstrdup(key) : NULL;
    m->nav[m->nnav].depth = depth;
    m->nav[m->nnav].count = count;
    m->nnav++;
}

static int depth_of(const char *s)
{
    int d = 0;

    for (; *s; s++)
        d += *s == '/';
    return d;
}

static const char *last_part(const char *s)
{
    const char *p = strrchr(s, '/');

    return p ? p + 1 : s;
}

void model_set_mode(Model *m, NavMode mode)
{
    size_t cap = 0;
    Strs s = { NULL, 0, 0 };

    nav_free(m);
    m->mode = mode;
    nav_add(m, &cap, "Totes les fitxes", NULL, 0, m->n);
    if (mode == NAV_CARPETES) {
        size_t arrel = 0;

        for (size_t i = 0; i < m->n; i++) {
            if (m->notes[i].carpeta[0])
                strs_add(&s, m->notes[i].carpeta);
            else
                arrel++;
        }
        strs_tree(&s);
        for (size_t i = 0; i < s.n; i++) {
            size_t c = 0;

            for (size_t k = 0; k < m->n; k++)
                c += under(m->notes[k].carpeta, s.v[i]);
            nav_add(m, &cap, last_part(s.v[i]), s.v[i], 1 + depth_of(s.v[i]), c);
        }
        if (arrel)
            nav_add(m, &cap, "(a l'arrel)", "", 1, arrel);
    } else if (mode == NAV_ETIQUETES) {
        for (size_t i = 0; i < m->ntags; i++)
            strs_add(&s, m->tags[i].tag);
        strs_tree(&s);
        for (size_t i = 0; i < s.n; i++) {
            size_t c = 0;
            long long last = -1;

            for (size_t k = 0; k < m->ntags; k++)       /* notes distintes */
                if (m->tags[k].fitxa != last && under(m->tags[k].tag, s.v[i])) {
                    c++;
                    last = m->tags[k].fitxa;
                }
            nav_add(m, &cap, last_part(s.v[i]), s.v[i], 1 + depth_of(s.v[i]), c);
        }
    } else {
        size_t sense = 0;

        for (size_t i = 0; i < m->n; i++) {
            if (m->notes[i].tipus[0])
                strs_add(&s, m->notes[i].tipus);
            else
                sense++;
        }
        qsort(s.v, s.n, sizeof *s.v, str_cmp);
        for (size_t i = 0; i < s.n;) {
            size_t j = i;

            while (j < s.n && !strcmp(s.v[j], s.v[i]))
                j++;
            nav_add(m, &cap, s.v[i], s.v[i], 1, j - i);
            i = j;
        }
        if (sense)
            nav_add(m, &cap, "(sense tipus)", "", 1, sense);
    }
    for (size_t i = 0; i < s.n; i++)
        free(s.v[i]);
    free(s.v);
}

/* ---------- llista ---------- */

static const Model *sort_model;
static SortMode     sort_mode;

static int note_cmp(const void *a, const void *b)
{
    const MNote *x = &sort_model->notes[*(const size_t *)a], *y = &sort_model->notes[*(const size_t *)b];
    int r = 0;

    switch (sort_mode) {
    case SORT_DATA:                     /* les que no en tenen, al final */
        if (!x->data[0] != !y->data[0])
            return x->data[0] ? -1 : 1;
        r = strcmp(x->data, y->data);
        break;
    case SORT_MODIF:
        r = x->mtime > y->mtime ? -1 : x->mtime < y->mtime;
        break;
    case SORT_PARAULES:
        r = x->paraules > y->paraules ? -1 : x->paraules < y->paraules;
        break;
    default:
        break;
    }
    if (!r)
        r = strcmp(x->fold, y->fold);
    return r ? r : strcmp(x->cami, y->cami);
}

size_t model_list(const Model *m, const MNav *nav, const char *filter, SortMode sort, size_t **out)
{
    size_t *v = xmalloc((m->n ? m->n : 1) * sizeof *v), n = 0;
    unsigned char *ok = NULL;

    if (nav && nav->key && m->mode == NAV_ETIQUETES) {
        ok = calloc(m->n ? m->n : 1, 1);
        for (size_t k = 0; k < m->ntags; k++)
            if (under(m->tags[k].tag, nav->key)) {
                long i = model_find(m, m->tags[k].fitxa);

                if (i >= 0)
                    ok[i] = 1;
            }
    }
    for (size_t i = 0; i < m->n; i++) {
        const MNote *x = &m->notes[i];

        if (nav && nav->key) {
            if (m->mode == NAV_CARPETES &&
                !(nav->key[0] ? under(x->carpeta, nav->key) : !x->carpeta[0]))
                continue;
            if (m->mode == NAV_TIPUS && strcmp(x->tipus, nav->key))
                continue;
            if (m->mode == NAV_ETIQUETES && !(ok && ok[i]))
                continue;
        }
        if (filter && *filter && !strstr(x->fold, filter))
            continue;
        v[n++] = i;
    }
    free(ok);
    sort_model = m;
    sort_mode = sort;
    qsort(v, n, sizeof *v, note_cmp);
    *out = v;
    return n;
}

const char *model_sort_label(SortMode s)
{
    static const char *const l[] = { "per nom", "per data", "per modificació", "per paraules" };

    return s < SORT_N ? l[s] : "";
}

const char *model_mode_label(NavMode mode)
{
    static const char *const l[] = { "Carpetes", "Etiquetes", "Tipus" };

    return mode < NAV_N ? l[mode] : "";
}

char model_type_letter(const char *t)
{
    if (!t || !*t)
        return ' ';
    if (!strcmp(t, "nota de camp"))
        return 'N';
    if (!strncmp(t, "bibliografi", 11))
        return 'B';
    if (!strncmp(t, "instituci", 9))
        return 'I';
    if (t[0] >= 'a' && t[0] <= 'z')
        return (char)(t[0] - 32);
    if ((unsigned char)t[0] < 0x80)
        return t[0];
    return '*';
}
