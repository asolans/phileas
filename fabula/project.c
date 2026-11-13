#include "project.h"
#include "fold.h"
#include "yaml.h"

#include "buffer.h"
#include "outline.h"
#include "utf8.h"
#include "util.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wctype.h>

#define HEAD_MAX 4096
#define FILE_MAX (64L * 1024 * 1024)

/* ---------- utilitats ---------- */

int fk_name_cmp(const char *a, const char *b)
{
    while (*a && *b) {
        if (isdigit((unsigned char)*a) && isdigit((unsigned char)*b)) {
            const char *ea = a, *eb = b;
            size_t la, lb;
            int c;

            while (*a == '0' && isdigit((unsigned char)a[1]))
                a++;
            while (*b == '0' && isdigit((unsigned char)b[1]))
                b++;
            ea = a;
            eb = b;
            while (isdigit((unsigned char)*ea))
                ea++;
            while (isdigit((unsigned char)*eb))
                eb++;
            la = (size_t)(ea - a);
            lb = (size_t)(eb - b);
            if (la != lb)
                return la < lb ? -1 : 1;
            c = strncmp(a, b, la);
            if (c)
                return c < 0 ? -1 : 1;
            a = ea;
            b = eb;
            continue;
        }
        {
            int ca = tolower((unsigned char)*a), cb = tolower((unsigned char)*b);

            if (ca != cb)
                return ca < cb ? -1 : 1;
        }
        a++;
        b++;
    }
    if (*a || *b)
        return *a ? 1 : -1;
    return 0;
}


char *fk_slug(const char *title)
{
    size_t n = strlen(title), i = 0, o = 0;
    char *out = xmalloc(3 * n + 16);
    int dash = 0;

    while (i < n) {
        wchar_t c;
        size_t k = utf8_decode(title, n, i, &c);
        const char *rep;

        i += k ? k : 1;
        if (c == UTF8_INVALID)
            continue;
        c = cm_lower_char(c);
        rep = cm_strip_char(c);
        if (rep) {
            for (; *rep; rep++)
                out[o++] = *rep;
            dash = 0;
        } else if (c < 0x80 && isalnum((int)c)) {
            out[o++] = (char)c;
            dash = 0;
        } else if (c >= 0x80) {
            continue;   /* altres alfabets: es descarten */
        } else if (o && !dash) {
            out[o++] = '-';
            dash = 1;
        }
    }
    while (o && out[o - 1] == '-')
        o--;
    if (!o) {
        strcpy(out, "sense-titol");
        return out;
    }
    out[o] = '\0';
    return out;
}

/* Minúscules i sense accents (el que fa fk_slug, però conservant la resta) */
static char *foldstr(const char *s)
{
    return cm_fold(s, strlen(s));
}

void fk_fold_line(FoldBuf *fb, const char *s, size_t len)
{
    size_t i = 0, o = 0;

    if (fb->cap < 4 * len + 8) {
        fb->cap = 4 * len + 8;
        fb->f = xrealloc(fb->f, fb->cap);
        fb->s = xrealloc(fb->s, fb->cap * sizeof *fb->s);
        fb->e = xrealloc(fb->e, fb->cap * sizeof *fb->e);
    }
    while (i < len) {
        wchar_t c;
        size_t k, from = i, e;
        const char *rep;
        char enc[4];
        size_t el, before = o;

        if ((unsigned char)s[i] < 0x80) {            /* ASCII: el camí ràpid */
            fb->f[o] = (char)tolower((unsigned char)s[i]);
            fb->s[o] = i;
            fb->e[o++] = i + 1;
            i++;
            continue;
        }
        k = utf8_decode(s, len, i, &c);

        if (!k)
            k = 1;
        i += k;
        e = i;
        if (c == UTF8_INVALID) {
            fb->f[o] = s[from];             /* byte invàlid: es conserva tal qual */
            fb->s[o] = from;
            fb->e[o++] = e;
            continue;
        }
        c = cm_lower_char(c);
        rep = cm_strip_char(c);
        if (rep) {
            for (; *rep; rep++) {
                fb->f[o] = *rep;
                fb->s[o] = from;
                fb->e[o++] = e;
            }
            (void)before;
        } else {
            el = utf8_encode(c, enc);
            for (size_t j = 0; j < el; j++) {
                fb->f[o] = enc[j];
                fb->s[o] = from;
                fb->e[o++] = e;
            }
        }
    }
    fb->f[o] = '\0';
    fb->n = o;
}

void fk_foldbuf_free(FoldBuf *fb)
{
    free(fb->f);
    free(fb->s);
    free(fb->e);
    memset(fb, 0, sizeof *fb);
}

int fk_match(const char *text, const char *needle)
{
    char *t = foldstr(text), *q = foldstr(needle);
    int r = strstr(t, q) != NULL;

    free(t);
    free(q);
    return r;
}

int fk_node_match(const FNode *n, const char *needle)
{
    const char *f[] = { n->title, n->sinopsi, n->pov, n->lloc, n->data, n->pers, n->estat };

    for (size_t i = 0; i < sizeof f / sizeof *f; i++)
        if (f[i] && f[i][0] && fk_match(f[i], needle))
            return 1;
    return 0;
}

void fk_fmt_num(size_t n, char *out, size_t outsz)
{
    char tmp[32], res[48];
    size_t l, o = 0;

    snprintf(tmp, sizeof tmp, "%zu", n);
    l = strlen(tmp);
    for (size_t i = 0; i < l; i++) {
        if (i && (l - i) % 3 == 0)
            res[o++] = '.';
        res[o++] = tmp[i];
    }
    res[o] = '\0';
    snprintf(out, outsz, "%s", res);
}

static char *read_head(const char *path, size_t max, size_t *len)
{
    FILE *f = fopen(path, "rb");
    char *buf;

    *len = 0;
    if (!f)
        return NULL;
    buf = xmalloc(max + 1);
    *len = fread(buf, 1, max, f);
    fclose(f);
    buf[*len] = '\0';
    return buf;
}

static char *read_all(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    char *buf = NULL;
    size_t n = 0, cap = 0;

    if (!f)
        return NULL;
    for (;;) {
        size_t r;

        if (n + 4096 + 1 > cap)
            buf = xrealloc(buf, cap = cap ? cap * 2 : 8192);
        r = fread(buf + n, 1, cap - n - 1, f);
        n += r;
        if (r == 0)
            break;
        if ((long)n > FILE_MAX) {
            free(buf);
            fclose(f);
            errno = EFBIG;
            return NULL;
        }
    }
    fclose(f);
    buf[n] = '\0';
    *len = n;
    return buf;
}

static char *join(const char *a, const char *b)
{
    size_t la = strlen(a), lb = strlen(b);
    char *s = xmalloc(la + lb + 2);

    memcpy(s, a, la);
    s[la] = '/';
    memcpy(s + la + 1, b, lb + 1);
    return s;
}

/* ---------- nodes ---------- */

static FNode *node_new(FKind kind, FNode *parent)
{
    FNode *n = xmalloc(sizeof *n);

    memset(n, 0, sizeof *n);
    n->kind = kind;
    n->parent = parent;
    n->title = xstrdup("");
    n->pov = xstrdup("");
    n->lloc = xstrdup("");
    n->data = xstrdup("");
    n->pers = xstrdup("");
    n->alies = xstrdup("");
    if (parent) {
        parent->kids = xrealloc(parent->kids, (parent->nkids + 1) * sizeof *parent->kids);
        parent->kids[parent->nkids++] = n;
    }
    return n;
}

static void node_free(FNode *n)
{
    for (size_t i = 0; i < n->nkids; i++)
        node_free(n->kids[i]);
    free(n->kids);
    free(n->title);
    free(n->name);
    free(n->path);
    free(n->estat);
    free(n->sinopsi);
    free(n->pov);
    free(n->lloc);
    free(n->data);
    free(n->pers);
    free(n->alies);
    for (size_t i = 0; i < n->nlinks; i++) {
        free(n->links[i].target);
        free(n->links[i].context);
    }
    free(n->links);
    free(n);
}

static void set_str(char **dst, const char *s)
{
    free(*dst);
    *dst = xstrdup(s);
}

/* Títol per defecte d'un nom de fitxer: sense prefix numèric ni extensió */
static char *title_from_name(const char *name)
{
    const char *s = name;
    size_t n;
    char *t;

    while (isdigit((unsigned char)*s))
        s++;
    if (s != name && (*s == '-' || *s == '_' || *s == ' ' || *s == '.'))
        s++;
    else
        s = name;
    n = strlen(s);
    if (n > 3 && !strcmp(s + n - 3, ".md"))
        n -= 3;
    t = xmalloc(n + 1);
    for (size_t i = 0; i < n; i++)
        t[i] = (s[i] == '-' || s[i] == '_') ? ' ' : s[i];
    t[n] = '\0';
    if (n && (unsigned char)t[0] < 0x80)
        t[0] = (char)toupper((unsigned char)t[0]);
    return t;
}

static long long st_ns(const struct stat *st)
{
    return (long long)st->st_mtim.tv_sec * 1000000000LL + st->st_mtim.tv_nsec;
}

/* El fitxer ja carregat a l'arbre anterior, o NULL */
static const FNode *find_cached(const Project *old, const char *path)
{
    if (!old->top)
        return NULL;
    for (size_t g = 0; g < old->top->nkids; g++)
        for (size_t i = 0; i < old->top->kids[g]->nkids; i++)
            if (!strcmp(old->top->kids[g]->kids[i]->path, path))
                return old->top->kids[g]->kids[i];
    return NULL;
}

/* ---------- metadades ---------- */

/* Camps `clau: valor` d'un comentari `<!-- a: b; c: d -->` */
typedef struct {
    char  *key[10];
    char  *val[10];
    int    n;
} Fields;

static void fields_free(Fields *f)
{
    for (int i = 0; i < f->n; i++) {
        free(f->key[i]);
        free(f->val[i]);
    }
    f->n = 0;
}

static const char *fields_get(const Fields *f, const char *key)
{
    for (int i = 0; i < f->n; i++)
        if (!strcmp(f->key[i], key))
            return f->val[i];
    return NULL;
}

/* El text sense <!-- i -->, o NULL si la línia no és un comentari d'una línia */
static int comment_body(const char *s, size_t len, size_t *a, size_t *z)
{
    size_t i = 0, e = len;

    while (i < e && (s[i] == ' ' || s[i] == '\t'))
        i++;
    while (e > i && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r'))
        e--;
    if (e - i < 7 || strncmp(s + i, "<!--", 4) || strncmp(s + e - 3, "-->", 3))
        return 0;
    *a = i + 4;
    *z = e - 3;
    return 1;
}

static void fields_parse(const char *s, size_t len, Fields *f)
{
    size_t a, z;

    f->n = 0;
    if (!comment_body(s, len, &a, &z))
        return;
    for (size_t i = a; i < z && f->n < 10;) {
        size_t e = i, c;

        while (e < z && s[e] != ';')
            e++;
        for (c = i; c < e && s[c] != ':'; c++)
            ;
        if (c < e) {
            size_t ks = i, ke = c, vs = c + 1, ve = e;

            while (ks < ke && isspace((unsigned char)s[ks]))
                ks++;
            while (ke > ks && isspace((unsigned char)s[ke - 1]))
                ke--;
            while (vs < ve && isspace((unsigned char)s[vs]))
                vs++;
            while (ve > vs && isspace((unsigned char)s[ve - 1]))
                ve--;
            if (ke > ks) {
                char *k = xmalloc(ke - ks + 1), *v = xmalloc(ve - vs + 1);

                for (size_t j = 0; j < ke - ks; j++)
                    k[j] = (char)tolower((unsigned char)s[ks + j]);
                k[ke - ks] = '\0';
                memcpy(v, s + vs, ve - vs);
                v[ve - vs] = '\0';
                f->key[f->n] = k;
                f->val[f->n++] = v;
            }
        }
        i = e + 1;
    }
}

/* Paraules de prosa d'una línia: sense la capçalera YAML, els títols ni el
 * comentari de metadades de sota d'un títol */
static size_t prose_line_words(Buffer *b, size_t i)
{
    int k = MD_BASE(b->lines[i].kind);

    if (k == MD_META || k == MD_HEADING || k == MD_UNDERLINE)
        return 0;
    if (k == MD_TEXT && i + 1 < b->nlines && MD_BASE(b->lines[i + 1].kind) == MD_UNDERLINE)
        return 0;
    if (k == MD_HTML && i > 0) {
        int pk = MD_BASE(b->lines[i - 1].kind);
        size_t a, z;

        if ((pk == MD_HEADING || pk == MD_UNDERLINE) &&
            comment_body(b->lines[i].text, b->lines[i].len, &a, &z))
            return 0;
    }
    return buf_line_words(b, i);
}

static size_t prose_words(Buffer *b)
{
    size_t words = 0;

    buf_scan(b);
    for (size_t i = 0; i < b->nlines; i++)
        words += prose_line_words(b, i);
    return words;
}

static void meta_from_yaml(FNode *n, const char *head, size_t hl)
{
    char tmp[512];

    if (!head)
        return;
    if (yaml_get(head, hl, "pov", tmp, sizeof tmp))
        set_str(&n->pov, tmp);
    if (yaml_get(head, hl, "lloc", tmp, sizeof tmp))
        set_str(&n->lloc, tmp);
    if (yaml_get(head, hl, "data", tmp, sizeof tmp))
        set_str(&n->data, tmp);
    if (yaml_get(head, hl, "alies", tmp, sizeof tmp)) {
        char **items;
        size_t cnt = yaml_list(tmp, &items);

        if (cnt) {
            char joined[512] = "";

            for (size_t i = 0; i < cnt; i++) {
                if (i)
                    strncat(joined, ", ", sizeof joined - strlen(joined) - 1);
                strncat(joined, items[i], sizeof joined - strlen(joined) - 1);
            }
            set_str(&n->alies, joined);
            yaml_list_free(items, cnt);
        } else {
            set_str(&n->alies, tmp);
        }
    }
    if (yaml_get(head, hl, "personatges", tmp, sizeof tmp)) {
        char **items;
        size_t cnt = yaml_list(tmp, &items);

        if (cnt) {
            char joined[512] = "";

            for (size_t i = 0; i < cnt; i++) {
                if (i)
                    strncat(joined, ", ", sizeof joined - strlen(joined) - 1);
                strncat(joined, items[i], sizeof joined - strlen(joined) - 1);
            }
            set_str(&n->pers, joined);
            yaml_list_free(items, cnt);
        } else {
            set_str(&n->pers, tmp);
        }
    }
}

/* Metadades d'una escena: el comentari de la línia següent a la capçalera, i
 * el que no hi diu s'hereta del capítol */
static void scene_meta(FNode *s, const FNode *ch, Buffer *b, size_t heading, const Project *p)
{
    Fields f;
    const char *v;

    memset(&f, 0, sizeof f);
    if (heading + 1 < b->nlines && MD_BASE(b->lines[heading + 1].kind) == MD_HTML)
        fields_parse(b->lines[heading + 1].text, b->lines[heading + 1].len, &f);
    set_str(&s->pov, (v = fields_get(&f, "pov")) ? v : ch->pov);
    if (v) s->own |= FK_OWN_POV;
    set_str(&s->lloc, (v = fields_get(&f, "lloc")) ? v : ch->lloc);
    if (v) s->own |= FK_OWN_LLOC;
    set_str(&s->data, (v = fields_get(&f, "data")) ? v : ch->data);
    if (v) s->own |= FK_OWN_DATA;
    set_str(&s->pers, (v = fields_get(&f, "personatges")) ? v : ch->pers);
    if (v) s->own |= FK_OWN_PERS;
    if ((v = fields_get(&f, "sinopsi"))) {
        set_str(&s->sinopsi, v);
        s->own |= FK_OWN_SINOPSI;
    } else {
        set_str(&s->sinopsi, "");
    }
    if ((v = fields_get(&f, "estat")) && v[0]) {
        set_str(&s->estat, v);
        s->estat_set = 1;
    } else if (ch->estat) {
        set_str(&s->estat, ch->estat);
        s->estat_set = 0;
    } else if (p->nestats) {
        s->estat = xstrdup(p->estats[0]);
    }
    fields_free(&f);
}

/* ---------- enllaços [[ ]] ---------- */

static void add_link(FNode *n, size_t line, size_t scene, const char *target, size_t tl,
                     const char *ctx, size_t cl)
{
    FLink *l;

    n->links = xrealloc(n->links, (n->nlinks + 1) * sizeof *n->links);
    l = &n->links[n->nlinks++];
    l->line = line;
    l->scene = scene;
    l->target = xmalloc(tl + 1);
    memcpy(l->target, target, tl);
    l->target[tl] = '\0';
    l->context = xmalloc(cl + 1);
    memcpy(l->context, ctx, cl);
    l->context[cl] = '\0';
    l->to = NULL;
}

/* Fragment del text al voltant de [a, b): fins a 30 caràcters abans i 40
 * després, amb «…» si es talla */
static char *link_context(const char *s, size_t len, size_t a, size_t b, size_t *outlen)
{
    size_t from = a, to = b;
    int before = 0, after = 0;
    char *out;
    size_t o = 0;

    while (from > 0 && before < 30) {
        from = utf8_prev(s, from);
        before++;
    }
    while (to < len && after < 40) {
        size_t nx = utf8_next(s, len, to);

        to = nx > to ? nx : to + 1;
        after++;
    }
    out = xmalloc(len + 16);
    if (from > 0)
        o += (size_t)sprintf(out + o, "…");
    memcpy(out + o, s + from, to - from);
    o += to - from;
    if (to < len)
        o += (size_t)sprintf(out + o, "…");
    out[o] = '\0';
    *outlen = o;
    return out;
}

/* Cerca els [[ ]] de cada línia del buffer (sense metadades, codi ni comentaris) */
static void extract_links(FNode *n, Buffer *b, const Outline *o)
{
    for (size_t i = 0; i < b->nlines; i++) {
        const char *s = b->lines[i].text;
        size_t len = b->lines[i].len, pos = 0;
        int k = MD_BASE(b->lines[i].kind);
        size_t scene = 0;

        if (k == MD_META || k == MD_CODE || k == MD_FENCE_OPEN || k == MD_FENCE_CLOSE)
            continue;
        if (k == MD_HTML) {
            size_t a, z;

            if (comment_body(s, len, &a, &z))
                continue;
        }
        for (size_t sc = 0, idx = 0; sc < o->n; sc++)
            if (o->items[sc].level == 3) {
                idx++;
                if (i > o->items[sc].line && i < outline_end(o, sc, b->nlines))
                    scene = idx;
            }
        while (pos + 4 <= len) {
            const char *open = memmem(s + pos, len - pos, "[[", 2), *close;
            size_t a, z, ts, te;

            if (!open)
                break;
            a = (size_t)(open - s);
            close = memmem(s + a + 2, len - a - 2, "]]", 2);
            if (!close)
                break;
            z = (size_t)(close - s) + 2;
            ts = a + 2;
            te = z - 2;
            /* el nom acaba a | o # */
            for (size_t j = ts; j < te; j++)
                if (s[j] == '|' || s[j] == '#') {
                    te = j;
                    break;
                }
            while (ts < te && isspace((unsigned char)s[ts]))
                ts++;
            while (te > ts && isspace((unsigned char)s[te - 1]))
                te--;
            if (te > ts && z - a < 240 && !memmem(s + a + 2, z - a - 4, "[[", 2)) {
                size_t cl;
                char *ctx = link_context(s, len, a, z, &cl);

                add_link(n, i, scene, s + ts, te - ts, ctx, cl);
                free(ctx);
            }
            pos = z;
        }
    }
}

static void copy_links(FNode *dst, const FNode *src)
{
    for (size_t i = 0; i < src->nlinks; i++)
        add_link(dst, src->links[i].line, src->links[i].scene, src->links[i].target,
                 strlen(src->links[i].target), src->links[i].context, strlen(src->links[i].context));
    set_str(&dst->alies, src->alies);
}

typedef struct {
    char   *key;
    FNode  *node;
    size_t  seq;
} KeyEnt;

static int cmp_keys(const void *a, const void *b)
{
    const KeyEnt *x = a, *y = b;
    int r = strcmp(x->key, y->key);

    return r ? r : (x->seq < y->seq ? -1 : x->seq > y->seq);
}

static int cmp_key_only(const void *a, const void *b)
{
    return strcmp(((const KeyEnt *)a)->key, ((const KeyEnt *)b)->key);
}

static void add_key(KeyEnt **v, size_t *n, size_t *cap, const char *text, FNode *node)
{
    char *k = foldstr(text);

    if (!k[0]) {
        free(k);
        return;
    }
    if (*n == *cap)
        *v = xrealloc(*v, (*cap = *cap ? *cap * 2 : 64) * sizeof **v);
    (*v)[*n].key = k;
    (*v)[*n].node = node;
    (*v)[*n].seq = *n;
    (*n)++;
}

/* Resol cada enllaç a la fitxa o capítol que té aquest nom: primer les
 * fitxes, i dins de cada una, el títol, el nom del fitxer i els àlies */
static void resolve_links(Project *p)
{
    KeyEnt *v = NULL;
    size_t n = 0, cap = 0;

    if (!p->top)
        return;
    for (int pass = 0; pass < 2; pass++)           /* 0: fitxes; 1: capítols */
        for (size_t g = 0; g < p->top->nkids; g++)
            for (size_t i = 0; i < p->top->kids[g]->nkids; i++) {
                FNode *f = p->top->kids[g]->kids[i];
                char base[300];
                size_t bl;

                if ((pass == 0) != (f->kind == FK_CARD))
                    continue;
                add_key(&v, &n, &cap, f->title, f);
                snprintf(base, sizeof base, "%s", f->name);
                bl = strlen(base);
                if (bl > 3 && !strcmp(base + bl - 3, ".md"))
                    base[bl - 3] = '\0';
                add_key(&v, &n, &cap, base, f);
                if (f->alies && f->alies[0]) {
                    char *copy = xstrdup(f->alies), *save = NULL;

                    for (char *t = strtok_r(copy, ",", &save); t; t = strtok_r(NULL, ",", &save)) {
                        while (*t == ' ')
                            t++;
                        add_key(&v, &n, &cap, t, f);
                    }
                    free(copy);
                }
            }
    if (n)
        qsort(v, n, sizeof *v, cmp_keys);
    for (size_t g = 0; g < p->top->nkids; g++)
        for (size_t i = 0; i < p->top->kids[g]->nkids; i++) {
            FNode *f = p->top->kids[g]->kids[i];

            for (size_t k = 0; k < f->nlinks; k++) {
                KeyEnt probe, *hit;

                probe.key = foldstr(f->links[k].target);
                hit = n ? bsearch(&probe, v, n, sizeof *v, cmp_key_only) : NULL;
                f->links[k].to = hit ? hit->node : NULL;
                free(probe.key);
            }
        }
    for (size_t i = 0; i < n; i++)
        free(v[i].key);
    free(v);
}

/* Còpia de les dades llegides d'un node (per a la memòria cau) */
static void copy_node_data(FNode *dst, const FNode *src, const Project *p)
{
    set_str(&dst->title, src->title);
    set_str(&dst->sinopsi, src->sinopsi ? src->sinopsi : "");
    set_str(&dst->pov, src->pov);
    set_str(&dst->lloc, src->lloc);
    set_str(&dst->data, src->data);
    set_str(&dst->pers, src->pers);
    dst->own = src->own;
    dst->words = src->words;
    dst->line = src->line;
    if (src->estat_set) {
        set_str(&dst->estat, src->estat);
        dst->estat_set = 1;
    } else if (src->kind == FK_SCENE && src->estat) {
        set_str(&dst->estat, src->estat);    /* heretat: el capítol ja el tindrà */
    } else if (p->nestats) {
        set_str(&dst->estat, p->estats[0]);
    }
}

/* Carrega un fitxer .md com a capítol o fitxa; no falla mai: si no es pot
 * llegir, queda amb 0 paraules. */
static FNode *load_file(const Project *p, const Project *old, FNode *parent, const char *dir,
                        const char *name, FKind kind)
{
    FNode *n = node_new(kind, parent);
    char *head, tmp[512];
    size_t hl;
    Buffer b;
    Outline o;

    n->name = xstrdup(name);
    n->path = join(dir, name);
    {
        struct stat st;

        if (stat(n->path, &st) == 0) {
            n->mtime = st_ns(&st);
            n->fsize = st.st_size;
            n->ino = st.st_ino;
        }
    }
    if (old) {
        const FNode *c = find_cached(old, n->path);

        if (c && c->kind == kind && c->mtime == n->mtime && c->fsize == n->fsize &&
            c->ino == n->ino) {
            copy_node_data(n, c, p);
            copy_links(n, c);
            for (size_t i = 0; i < c->nkids; i++) {
                FNode *s = node_new(FK_SCENE, n);

                copy_node_data(s, c->kids[i], p);
                if (!c->kids[i]->estat_set) {            /* hereta l'estat del capítol */
                    free(s->estat);
                    s->estat = n->estat ? xstrdup(n->estat) : NULL;
                }
            }
            return n;
        }
    }
    head = read_head(n->path, HEAD_MAX, &hl);
    if (head && yaml_get(head, hl, "estat", tmp, sizeof tmp) && tmp[0]) {
        set_str(&n->estat, tmp);
        n->estat_set = 1;
    } else if (p->nestats) {
        n->estat = xstrdup(p->estats[0]);
    }
    if (head && yaml_get(head, hl, "sinopsi", tmp, sizeof tmp))
        set_str(&n->sinopsi, tmp);
    else
        set_str(&n->sinopsi, "");
    if (head && yaml_get(head, hl, "titol", tmp, sizeof tmp) && tmp[0])
        set_str(&n->title, tmp);
    meta_from_yaml(n, head, hl);
    free(head);

    buf_init(&b);
    if (buf_load(&b, n->path) != 0) {
        buf_free(&b);
        if (!n->title[0]) {
            free(n->title);
            n->title = title_from_name(name);
        }
        return n;
    }
    n->words = prose_words(&b);

    outline_build(&b, &o);
    extract_links(n, &b, &o);
    if (!n->title[0])
        for (size_t k = 0; k < o.n; k++)
            if (o.items[k].level <= 2 && o.items[k].title && o.items[k].title[0]) {
                set_str(&n->title, o.items[k].title);
                break;
            }
    if (!n->title[0]) {
        free(n->title);
        n->title = title_from_name(name);
    }
    if (kind == FK_CHAPTER)
        for (size_t k = 0; k < o.n; k++)
            if (o.items[k].level == 3) {
                FNode *s = node_new(FK_SCENE, n);
                size_t end = outline_end(&o, k, b.nlines);

                set_str(&s->title, o.items[k].title ? o.items[k].title : "");
                s->line = o.items[k].line;
                for (size_t i = s->line + 1; i < end; i++)
                    s->words += prose_line_words(&b, i);
                scene_meta(s, n, &b, s->line, p);
            }
    outline_free(&o);
    buf_free(&b);
    return n;
}

static int cmp_names(const void *a, const void *b)
{
    return fk_name_cmp(*(char *const *)a, *(char *const *)b);
}

static int group_rank(const char *name)
{
    static const char *const known[] = { "manuscrit", "personatges", "llocs", "recerca", "notes" };

    for (int i = 0; i < 5; i++)
        if (!strcasecmp(name, known[i]))
            return i;
    return 5;
}

static int cmp_groups(const void *a, const void *b)
{
    const char *x = *(char *const *)a, *y = *(char *const *)b;
    int rx = group_rank(x), ry = group_rank(y);

    return rx != ry ? rx - ry : fk_name_cmp(x, y);
}

/* Noms dels elements d'una carpeta: només fitxers .md (files = 1) o només
 * carpetes (files = 0); mai enllaços simbòlics ni noms amb punt. */
static size_t list_dir(const char *dir, int files, char ***names)
{
    DIR *d = opendir(dir);
    struct dirent *e;
    size_t n = 0, cap = 0;
    char **v = NULL;

    *names = NULL;
    if (!d)
        return 0;
    while ((e = readdir(d))) {
        struct stat st;
        char *path;
        size_t l = strlen(e->d_name);

        if (e->d_name[0] == '.')
            continue;
        path = join(dir, e->d_name);
        if (lstat(path, &st) != 0 || S_ISLNK(st.st_mode)) {
            free(path);
            continue;
        }
        free(path);
        if (files) {
            if (!S_ISREG(st.st_mode) || l < 4 || strcmp(e->d_name + l - 3, ".md"))
                continue;
        } else if (!S_ISDIR(st.st_mode) || !strcmp(e->d_name, "plantilles")) {
            continue;
        }
        if (n == cap)
            v = xrealloc(v, (cap = cap ? cap * 2 : 16) * sizeof *v);
        v[n++] = xstrdup(e->d_name);
    }
    closedir(d);
    *names = v;
    return n;
}

static void names_free(char **v, size_t n)
{
    for (size_t i = 0; i < n; i++)
        free(v[i]);
    free(v);
}

static void sum_words(FNode *g)
{
    g->words = 0;
    for (size_t i = 0; i < g->nkids; i++)
        g->words += g->kids[i]->words;
}

static int open_ex(Project *p, const char *dir, const Project *old, char *err, size_t errsz)
{
    char *pm, *file, tmp[512];
    size_t len, ng;
    char **groups;
    struct stat st;

    memset(p, 0, sizeof *p);
    if (stat(dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
        snprintf(err, errsz, "%s: no és una carpeta", dir);
        return -1;
    }
    {
        char *real = realpath(dir, NULL);

        p->root = real ? real : xstrdup(dir);
    }
    file = join(p->root, "projecte.md");
    pm = read_all(file, &len);
    free(file);
    if (!pm) {
        snprintf(err, errsz, "%s: no és un projecte de Fàbula (falta projecte.md)", dir);
        free(p->root);
        memset(p, 0, sizeof *p);
        return -1;
    }
    if (yaml_get(pm, len, "titol", tmp, sizeof tmp) && tmp[0])
        p->titol = xstrdup(tmp);
    else {
        const char *base = strrchr(p->root, '/');

        p->titol = title_from_name(base ? base + 1 : p->root);
    }
    p->autor = xstrdup(yaml_get(pm, len, "autor", tmp, sizeof tmp) ? tmp : "");
    p->idioma = xstrdup(yaml_get(pm, len, "idioma", tmp, sizeof tmp) && tmp[0] ? tmp : "ca");
    p->data_limit = xstrdup(yaml_get(pm, len, "data-limit", tmp, sizeof tmp) ? tmp : "");
    if (yaml_get(pm, len, "objectiu", tmp, sizeof tmp))
        p->objectiu = strtol(tmp, NULL, 10);
    if (p->objectiu < 0)
        p->objectiu = 0;
    if (yaml_get(pm, len, "estats", tmp, sizeof tmp))
        p->nestats = yaml_list(tmp, &p->estats);
    if (!p->nestats) {
        static const char *const def[] = { "esborrany", "revisat", "final" };

        p->estats = xmalloc(3 * sizeof *p->estats);
        for (int i = 0; i < 3; i++)
            p->estats[i] = xstrdup(def[i]);
        p->nestats = 3;
    }
    free(pm);

    p->top = node_new(FK_ROOT, NULL);
    ng = list_dir(p->root, 0, &groups);
    if (ng)
        qsort(groups, ng, sizeof *groups, cmp_groups);
    for (size_t g = 0; g < ng; g++) {
        FNode *gn = node_new(FK_GROUP, p->top);
        char *gdir = join(p->root, groups[g]);
        char **files;
        size_t nf = list_dir(gdir, 1, &files);
        int ms = !strcasecmp(groups[g], "manuscrit");

        gn->name = xstrdup(groups[g]);
        gn->path = gdir;
        free(gn->title);
        gn->title = xstrdup(groups[g]);
        if ((unsigned char)gn->title[0] < 0x80)
            gn->title[0] = (char)toupper((unsigned char)gn->title[0]);
        gn->open = ms;
        if (nf)
            qsort(files, nf, sizeof *files, cmp_names);
        for (size_t i = 0; i < nf; i++)
            load_file(p, old, gn, gdir, files[i], ms ? FK_CHAPTER : FK_CARD);
        sum_words(gn);
        if (ms)
            p->manuscrit = gn;
        names_free(files, nf);
    }
    names_free(groups, ng);
    resolve_links(p);
    return 0;
}

int project_open(Project *p, const char *dir, char *err, size_t errsz)
{
    return open_ex(p, dir, NULL, err, errsz);
}

int project_refresh(Project *p, char *err, size_t errsz)
{
    Project np;

    if (open_ex(&np, p->root, p, err, errsz) != 0)
        return -1;
    /* què és desplegat: pel nom dels grups i el camí dels capítols */
    for (size_t g = 0; g < np.top->nkids; g++) {
        FNode *ng = np.top->kids[g];

        for (size_t og = 0; og < p->top->nkids; og++) {
            FNode *ogn = p->top->kids[og];

            if (strcmp(ogn->name, ng->name))
                continue;
            ng->open = ogn->open;
            for (size_t i = 0; i < ng->nkids; i++) {
                const FNode *o = find_cached(p, ng->kids[i]->path);

                if (o)
                    ng->kids[i]->open = o->open;
            }
        }
    }
    project_free(p);
    *p = np;
    return 0;
}

void project_free(Project *p)
{
    if (p->top)
        node_free(p->top);
    free(p->root);
    free(p->titol);
    free(p->autor);
    free(p->idioma);
    free(p->data_limit);
    yaml_list_free(p->estats, p->nestats);
    memset(p, 0, sizeof *p);
}

size_t project_words(const Project *p)
{
    return p->manuscrit ? p->manuscrit->words : 0;
}

/* ---------- objectius ---------- */

static int parse_date(const char *s, struct tm *tm)
{
    int y, m, d;
    char c;

    if (sscanf(s, "%4d-%2d-%2d%c", &y, &m, &d, &c) != 3 || y < 1970 || m < 1 || m > 12 ||
        d < 1 || d > 31)
        return 0;
    memset(tm, 0, sizeof *tm);
    tm->tm_year = y - 1900;
    tm->tm_mon = m - 1;
    tm->tm_mday = d;
    tm->tm_hour = 12;
    tm->tm_isdst = -1;
    return 1;
}

long project_days_left(const Project *p, time_t now)
{
    struct tm lim, today;
    time_t a, b;

    if (!p->data_limit[0] || !parse_date(p->data_limit, &lim))
        return -1;
    localtime_r(&now, &today);
    today.tm_hour = 12;
    today.tm_min = today.tm_sec = 0;
    today.tm_isdst = -1;
    a = mktime(&today);
    b = mktime(&lim);
    if (b < a)
        return -1;
    return (long)((b - a + 3600 * 6) / 86400) + 1;
}

long project_daily_needed(const Project *p, time_t now)
{
    long left, days;

    if (p->objectiu <= 0)
        return -1;
    left = p->objectiu - (long)project_words(p);
    if (left <= 0)
        return 0;
    days = project_days_left(p, now);
    if (days <= 0)
        return -1;
    return (left + days - 1) / days;
}

/* ---------- estats ---------- */

const char *project_next_estat(const Project *p, const char *estat)
{
    if (!p->nestats)
        return NULL;
    for (size_t i = 0; i < p->nestats; i++)
        if (estat && !strcmp(p->estats[i], estat))
            return p->estats[(i + 1) % p->nestats];
    return p->estats[0];
}

static int write_atomic(const char *path, const char *data, size_t len)
{
    struct stat st;
    size_t pl = strlen(path);
    char *tmp = xmalloc(pl + 32);
    int fd, ok = 0;
    FILE *f;

    snprintf(tmp, pl + 32, "%s.fabula-tmp.%ld", path, (long)getpid());
    fd = open(tmp, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (fd < 0) {
        free(tmp);
        return -1;
    }
    if (stat(path, &st) == 0)
        (void)fchmod(fd, st.st_mode & 07777);
    f = fdopen(fd, "wb");
    if (!f) {
        close(fd);
        unlink(tmp);
        free(tmp);
        return -1;
    }
    if (fwrite(data, 1, len, f) == len && fflush(f) == 0 && fsync(fd) == 0 && fclose(f) == 0) {
        f = NULL;
        ok = rename(tmp, path) == 0;
    }
    if (f)
        fclose(f);
    if (!ok) {
        int e = errno;

        unlink(tmp);
        errno = e;
    }
    free(tmp);
    return ok ? 0 : -1;
}

int project_set_estat(Project *p, FNode *n, const char *estat)
{
    char *text, *res;
    size_t len, rl;
    int rc;

    (void)p;
    if ((n->kind != FK_CHAPTER && n->kind != FK_CARD) || !n->path) {
        errno = EINVAL;
        return -1;
    }
    text = read_all(n->path, &len);
    if (!text)
        return -1;
    res = yaml_set(text, len, "estat", estat, 0, &rl);
    free(text);
    rc = write_atomic(n->path, res, rl);
    free(res);
    if (rc != 0)
        return -1;
    set_str(&n->estat, estat);
    n->estat_set = 1;
    {
        struct stat st;

        if (stat(n->path, &st) == 0) {
            n->mtime = st_ns(&st);
            n->fsize = st.st_size;
            n->ino = st.st_ino;
        }
    }
    return 0;
}

/* ---------- operacions sobre els fitxers ---------- */

static const char *const TPL_CHAPTER =
    "---\nestat: esborrany\nsinopsi:\n---\n\n## {{titol}}\n\n### Escena 1\n\n";
static const char *const TPL_PERSON =
    "---\ntipus: personatge\nsinopsi:\nalies: []\netiquetes: []\n---\n\n# {{titol}}\n\n"
    "## Aspecte\n\n## Personalitat\n\n## Motivació\n\n## Relacions\n\n## Arc\n\n";
static const char *const TPL_PLACE =
    "---\ntipus: lloc\nsinopsi:\netiquetes: []\n---\n\n# {{titol}}\n\n"
    "## Descripció\n\n## Història\n\n## Escenes\n\n";
static const char *const TPL_RESEARCH =
    "---\ntipus: recerca\nsinopsi:\nfont:\netiquetes: []\n---\n\n# {{titol}}\n\n## Notes\n\n";
static const char *const TPL_NOTE = "---\ntipus: nota\nsinopsi:\n---\n\n# {{titol}}\n\n";

/* Text amb {{titol}} i {{data}} substituïts (malloc) */
static char *expand(const char *tpl, const char *title)
{
    char date[16];
    time_t now = time(NULL);
    struct tm tm;
    size_t cap = strlen(tpl) + 4 * strlen(title) + 64, o = 0;
    char *out = xmalloc(cap);

    localtime_r(&now, &tm);
    strftime(date, sizeof date, "%Y-%m-%d", &tm);
    for (const char *s = tpl; *s;) {
        const char *rep = NULL;
        size_t sl = 0;

        if (!strncmp(s, "{{titol}}", 9))
            rep = title, sl = 9;
        else if (!strncmp(s, "{{data}}", 8))
            rep = date, sl = 8;
        if (rep) {
            size_t rl = strlen(rep);

            if (o + rl + strlen(s) + 8 > cap)
                out = xrealloc(out, cap = o + rl + strlen(s) + 64);
            memcpy(out + o, rep, rl);
            o += rl;
            s += sl;
        } else {
            if (o + 2 > cap)
                out = xrealloc(out, cap *= 2);
            out[o++] = *s++;
        }
    }
    out[o] = '\0';
    return out;
}

/* Títol net: sense espais als extrems ni salts de línia; 0 si no val */
static int clean_title(const char *in, char *out, size_t osz)
{
    size_t a = 0, b = strlen(in), o = 0;

    while (a < b && isspace((unsigned char)in[a]))
        a++;
    while (b > a && isspace((unsigned char)in[b - 1]))
        b--;
    for (size_t i = a; i < b && o + 1 < osz; i++)
        out[o++] = (in[i] == '\n' || in[i] == '\r' || in[i] == '\t') ? ' ' : in[i];
    out[o] = '\0';
    return o > 0 && b - a < osz;
}

static int exists(const char *path)
{
    struct stat st;

    return lstat(path, &st) == 0;
}

static int create_excl(const char *path, const char *text)
{
    int fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0666);
    size_t n = strlen(text);
    ssize_t w;

    if (fd < 0)
        return -1;
    w = write(fd, text, n);
    if (w != (ssize_t)n || fsync(fd) != 0) {
        int e = errno;

        close(fd);
        unlink(path);
        errno = e;
        return -1;
    }
    return close(fd);
}

static void fail(char *err, size_t esz, const char *what)
{
    snprintf(err, esz, "%s: %s", what, strerror(errno));
}

/* Longitud del prefix numèric d'un nom («02-» → 3), o 0 */
static size_t num_prefix(const char *name, size_t *digits)
{
    size_t d = 0;

    while (isdigit((unsigned char)name[d]))
        d++;
    if (digits)
        *digits = d;
    if (!d)
        return 0;
    return (name[d] == '-' || name[d] == '_' || name[d] == ' ' || name[d] == '.') ? d + 1 : d;
}

int project_new_item(Project *p, FNode *group, const char *title, char *path_out, size_t psz,
                     char *err, size_t esz)
{
    char t[256], *gdir, *tpl_path, *tpl_text = NULL, *body, *slug, name[320], *path;
    const char *tpl, *tname;
    int chapter, rc = -1;
    long next = 10;

    if (!clean_title(title, t, sizeof t)) {
        snprintf(err, esz, "El títol és buit o massa llarg.");
        return -1;
    }
    chapter = !group || group == p->manuscrit;
    if (chapter) {
        gdir = join(p->root, "manuscrit");
        if (!p->manuscrit && mkdir(gdir, 0777) != 0 && errno != EEXIST) {
            fail(err, esz, "No s'ha pogut crear manuscrit/");
            free(gdir);
            return -1;
        }
        tpl = TPL_CHAPTER;
        tname = "capitol";
        if (p->manuscrit)
            for (size_t i = 0; i < p->manuscrit->nkids; i++) {
                long v = strtol(p->manuscrit->kids[i]->name, NULL, 10);

                if (num_prefix(p->manuscrit->kids[i]->name, NULL) && v >= next)
                    next = (v / 10 + 1) * 10;
            }
    } else {
        const char *g = group->name;

        gdir = xstrdup(group->path);
        if (!strcasecmp(g, "personatges"))
            tpl = TPL_PERSON, tname = "personatge";
        else if (!strcasecmp(g, "llocs"))
            tpl = TPL_PLACE, tname = "lloc";
        else if (!strcasecmp(g, "recerca"))
            tpl = TPL_RESEARCH, tname = "recerca";
        else
            tpl = TPL_NOTE, tname = "nota";
    }
    {
        char rel[64];
        size_t tl;

        snprintf(rel, sizeof rel, "plantilles/%s.md", tname);
        tpl_path = join(p->root, rel);
        tpl_text = read_all(tpl_path, &tl);
        free(tpl_path);
        if (tpl_text && tl > 1024 * 1024) {
            free(tpl_text);
            tpl_text = NULL;
        }
    }
    body = expand(tpl_text ? tpl_text : tpl, t);
    free(tpl_text);
    slug = fk_slug(t);
    for (int k = 1; k < 100; k++) {
        char suffix[16] = "";

        if (k > 1)
            snprintf(suffix, sizeof suffix, "-%d", k);
        if (chapter)
            snprintf(name, sizeof name, "%02ld-%s%s.md", next, slug, suffix);
        else
            snprintf(name, sizeof name, "%s%s.md", slug, suffix);
        path = join(gdir, name);
        if (create_excl(path, body) == 0) {
            snprintf(path_out, psz, "%s", path);
            rc = 0;
            free(path);
            break;
        }
        if (errno != EEXIST) {
            fail(err, esz, "No s'ha pogut crear el fitxer");
            free(path);
            break;
        }
        free(path);
        if (k == 99)
            snprintf(err, esz, "Hi ha massa fitxers amb aquest nom.");
    }
    free(slug);
    free(body);
    free(gdir);
    return rc;
}

int project_rename(Project *p, FNode *n, const char *title, char *path_out, size_t psz,
                   char *err, size_t esz)
{
    char t[256], *text, *res, *slug, name[320], *dir, *npath, *slash;
    size_t len, rl, pl = 0;
    int rc;

    (void)p;
    if ((n->kind != FK_CHAPTER && n->kind != FK_CARD) || !n->path) {
        snprintf(err, esz, "Només es reanomenen capítols i fitxes.");
        return -1;
    }
    if (!clean_title(title, t, sizeof t)) {
        snprintf(err, esz, "El títol és buit o massa llarg.");
        return -1;
    }
    text = read_all(n->path, &len);
    if (!text) {
        fail(err, esz, "No s'ha pogut llegir");
        return -1;
    }
    res = yaml_set(text, len, "titol", t, 0, &rl);
    free(text);
    rc = write_atomic(n->path, res, rl);
    free(res);
    if (rc != 0) {
        fail(err, esz, "No s'ha pogut desar");
        return -1;
    }
    /* nom nou: el mateix prefix numèric (capítols) i el títol com a nom */
    slug = fk_slug(t);
    if (n->kind == FK_CHAPTER)
        pl = num_prefix(n->name, NULL);
    snprintf(name, sizeof name, "%.*s%s.md", (int)pl, n->name, slug);
    free(slug);
    snprintf(path_out, psz, "%s", n->path);
    if (!strcmp(name, n->name))
        return 0;
    dir = xstrdup(n->path);
    slash = strrchr(dir, '/');
    if (slash)
        *slash = '\0';
    npath = join(dir, name);
    free(dir);
    if (exists(npath)) {
        snprintf(err, esz, "El títol s'ha canviat, però ja hi ha un fitxer anomenat %s.", name);
        free(npath);
        return -1;
    }
    if (rename(n->path, npath) != 0) {
        fail(err, esz, "El títol s'ha canviat, però no s'ha pogut reanomenar el fitxer");
        free(npath);
        return -1;
    }
    snprintf(path_out, psz, "%s", npath);
    free(npath);
    return 0;
}

/* Mou path a .paperera/AAAAMMDD-HHMMSS-nom (el nom, si cal, amb un número) */
static int trash_path(const char *root, const char *path, const char *name, char *err, size_t esz)
{
    char *tdir, stamp[32], tname[400], *dst;
    time_t now = time(NULL);
    struct tm tm;

    tdir = join(root, ".paperera");
    if (mkdir(tdir, 0777) != 0 && errno != EEXIST) {
        fail(err, esz, "No s'ha pogut crear .paperera/");
        free(tdir);
        return -1;
    }
    localtime_r(&now, &tm);
    strftime(stamp, sizeof stamp, "%Y%m%d-%H%M%S", &tm);
    for (int k = 0; k < 100; k++) {
        if (k)
            snprintf(tname, sizeof tname, "%s-%d-%s", stamp, k, name);
        else
            snprintf(tname, sizeof tname, "%s-%s", stamp, name);
        dst = join(tdir, tname);
        if (!exists(dst)) {
            int rc = rename(path, dst);

            free(dst);
            free(tdir);
            if (rc != 0) {
                fail(err, esz, "No s'ha pogut moure a la paperera");
                return -1;
            }
            return 0;
        }
        free(dst);
    }
    free(tdir);
    snprintf(err, esz, "No s'ha pogut triar un nom a la paperera.");
    return -1;
}

int project_trash(Project *p, FNode *n, char *err, size_t esz)
{
    if ((n->kind != FK_CHAPTER && n->kind != FK_CARD) || !n->path) {
        snprintf(err, esz, "Només es mouen a la paperera capítols i fitxes.");
        return -1;
    }
    return trash_path(p->root, n->path, n->name, err, esz);
}

int project_move(Project *p, FNode *n, int dir, char *path_out, size_t psz, char *err, size_t esz)
{
    FNode *g = p->manuscrit, **order;
    size_t cnt, idx = (size_t)-1, other;
    char **target;
    int swap_ok = 0, rc = -1;

    if (n->kind != FK_CHAPTER || !g) {
        snprintf(err, esz, "Només es reordenen els capítols del manuscrit.");
        return -1;
    }
    cnt = g->nkids;
    for (size_t i = 0; i < cnt; i++)
        if (g->kids[i] == n)
            idx = i;
    if (idx == (size_t)-1)
        return -1;
    if ((dir < 0 && idx == 0) || (dir > 0 && idx + 1 == cnt)) {
        snprintf(err, esz, dir < 0 ? "Ja és el primer capítol." : "Ja és l'últim capítol.");
        return -1;
    }
    other = dir < 0 ? idx - 1 : idx + 1;

    order = xmalloc(cnt * sizeof *order);
    target = xmalloc(cnt * sizeof *target);
    for (size_t i = 0; i < cnt; i++) {
        order[i] = g->kids[i];
        target[i] = NULL;
    }
    {
        size_t da, db, pa = num_prefix(g->kids[idx]->name, &da), pb = num_prefix(g->kids[other]->name, &db);

        swap_ok = pa && pb && strtol(g->kids[idx]->name, NULL, 10) != strtol(g->kids[other]->name, NULL, 10);
        if (swap_ok) {
            /* cada un porta el prefix de l'altre */
            char nm[400];

            snprintf(nm, sizeof nm, "%.*s%s", (int)pb, g->kids[other]->name, g->kids[idx]->name + pa);
            target[idx] = xstrdup(nm);
            snprintf(nm, sizeof nm, "%.*s%s", (int)pa, g->kids[idx]->name, g->kids[other]->name + pb);
            target[other] = xstrdup(nm);
        } else {
            /* renumera tots en passos de 10, amb el nou ordre */
            FNode *t = order[idx];

            order[idx] = order[other];
            order[other] = t;
            for (size_t i = 0; i < cnt; i++) {
                char nm[400];
                size_t pl = num_prefix(order[i]->name, NULL);

                snprintf(nm, sizeof nm, "%02zu-%s", (i + 1) * 10, order[i]->name + pl);
                target[i] = xstrdup(nm);
            }
        }
    }
    /* Els destins no poden xocar amb fitxers que no es mouen */
    {
        int clash = 0;

        for (size_t i = 0; i < cnt && !clash; i++) {
            size_t src = swap_ok ? i : (size_t)-1;
            char *dst;

            if (!target[i])
                continue;
            if (!swap_ok) {
                for (size_t j = 0; j < cnt; j++)
                    if (g->kids[j] == order[i])
                        src = j;
            }
            if (!strcmp(g->kids[src]->name, target[i]))
                continue;
            dst = join(g->path, target[i]);
            if (exists(dst)) {
                /* val si el destí és un altre dels que es mouen */
                int mine = 0;

                for (size_t j = 0; j < cnt; j++)
                    if (j != src && !strcmp(g->kids[j]->name, target[i]))
                        mine = 1;
                clash = !mine;
            }
            free(dst);
        }
        if (clash) {
            snprintf(err, esz, "Un dels noms nous ja existeix; no s'ha canviat res.");
            goto done;
        }
    }
    /* Dues fases: tots a un nom temporal i després al definitiu */
    {
        size_t *src = xmalloc(cnt * sizeof *src);
        char **tmp = xmalloc(cnt * sizeof *tmp);
        size_t moved = 0;
        int ok = 1;

        for (size_t i = 0; i < cnt; i++) {
            src[i] = (size_t)-1;
            tmp[i] = NULL;
            if (!target[i])
                continue;
            if (swap_ok)
                src[i] = i;
            else
                for (size_t j = 0; j < cnt; j++)
                    if (g->kids[j] == order[i])
                        src[i] = j;
            if (!strcmp(g->kids[src[i]]->name, target[i])) {
                free(target[i]);
                target[i] = NULL;
                continue;
            }
        }
        for (size_t i = 0; i < cnt && ok; i++) {
            char nm[64];

            if (!target[i])
                continue;
            snprintf(nm, sizeof nm, ".fabula-mv-%ld-%zu", (long)getpid(), i);
            tmp[i] = join(g->path, nm);
            if (rename(g->kids[src[i]]->path, tmp[i]) != 0)
                ok = 0;
            else
                moved++;
        }
        for (size_t i = 0; i < cnt && ok; i++) {
            char *dst;

            if (!target[i])
                continue;
            dst = join(g->path, target[i]);
            if (rename(tmp[i], dst) != 0)
                ok = 0;
            else if (order[i] == n || (swap_ok && g->kids[src[i]] == n))
                snprintf(path_out, psz, "%s", dst);
            free(dst);
        }
        if (!ok) {
            /* torna els que han quedat amb el nom temporal */
            for (size_t i = 0; i < cnt; i++)
                if (tmp[i] && exists(tmp[i]))
                    (void)rename(tmp[i], g->kids[src[i]]->path);
            fail(err, esz, "No s'han pogut reanomenar els capítols");
        } else {
            rc = 0;
        }
        (void)moved;
        for (size_t i = 0; i < cnt; i++)
            free(tmp[i]);
        free(tmp);
        free(src);
    }
done:
    for (size_t i = 0; i < cnt; i++)
        free(target[i]);
    free(target);
    free(order);
    return rc;
}

/* El títol tal com s'escriuria a l'YAML (amb cometes si cal) */
static char *format_title_yaml(const char *t)
{
    size_t rl;
    char *r = yaml_set("---\n---\n", 8, "t", t, 0, &rl), *v, *nl;

    v = strstr(r, "t: ");
    v = v ? v + 3 : r;
    nl = strchr(v, '\n');
    if (nl)
        *nl = '\0';
    v = xstrdup(v);
    free(r);
    return v;
}

int project_create(const char *parent, const char *title, char *path_out, size_t psz,
                   char *err, size_t esz)
{
    static const char *const groups[] = { "manuscrit", "personatges", "llocs", "recerca", "notes" };
    char t[256], *slug, *dir, *file, *text;
    DIR *d;
    size_t cap;

    if (!clean_title(title, t, sizeof t)) {
        snprintf(err, esz, "El títol és buit o massa llarg.");
        return -1;
    }
    slug = fk_slug(t);
    dir = join(parent, slug);
    free(slug);
    if (mkdir(dir, 0777) != 0) {
        if (errno != EEXIST) {
            fail(err, esz, "No s'ha pogut crear la carpeta");
            free(dir);
            return -1;
        }
        d = opendir(dir);
        if (d) {
            struct dirent *e;
            int empty = 1;

            while ((e = readdir(d)))
                if (strcmp(e->d_name, ".") && strcmp(e->d_name, ".."))
                    empty = 0;
            closedir(d);
            if (!empty) {
                snprintf(err, esz, "%s ja existeix i no és buida.", dir);
                free(dir);
                return -1;
            }
        }
    }
    for (size_t i = 0; i < sizeof groups / sizeof *groups; i++) {
        char *g = join(dir, groups[i]);

        if (mkdir(g, 0777) != 0 && errno != EEXIST) {
            fail(err, esz, "No s'ha pogut crear una carpeta del projecte");
            free(g);
            free(dir);
            return -1;
        }
        free(g);
    }
    cap = strlen(t) * 2 + 600;
    text = xmalloc(cap);
    {
        char *q = format_title_yaml(t);

        snprintf(text, cap,
                 "---\ntitol: %s\nautor:\nidioma: ca\n"
                 "# objectiu: paraules del manuscrit (0 = sense objectiu)\nobjectiu: 0\n"
                 "# data-limit: AAAA-MM-DD\ndata-limit:\n"
                 "estats: [esborrany, revisat, final]\n---\n\n"
                 "Tauler del projecte: idees de conjunt, pendents i notes generals.\n", q);
        free(q);
    }
    file = join(dir, "projecte.md");
    if (create_excl(file, text) != 0 && errno != EEXIST) {
        fail(err, esz, "No s'ha pogut crear projecte.md");
        free(file);
        free(text);
        free(dir);
        return -1;
    }
    free(file);
    free(text);
    snprintf(path_out, psz, "%s", dir);
    free(dir);
    return 0;
}

/* ---------- instantànies ---------- */

#define STAMP_LEN 15

static void make_stamp(time_t t, char out[16])
{
    struct tm tm;

    localtime_r(&t, &tm);
    strftime(out, 16, "%Y%m%d-%H%M%S", &tm);
}

static int parse_stamp(const char *s, time_t *out)
{
    struct tm tm;
    int y, mo, d, h, mi, se;

    for (int i = 0; i < STAMP_LEN; i++)
        if (i == 8 ? s[i] != '-' : !isdigit((unsigned char)s[i]))
            return 0;
    if (sscanf(s, "%4d%2d%2d-%2d%2d%2d", &y, &mo, &d, &h, &mi, &se) != 6)
        return 0;
    memset(&tm, 0, sizeof tm);
    tm.tm_year = y - 1900;
    tm.tm_mon = mo - 1;
    tm.tm_mday = d;
    tm.tm_hour = h;
    tm.tm_min = mi;
    tm.tm_sec = se;
    tm.tm_isdst = -1;
    *out = mktime(&tm);
    return 1;
}

/* Carpeta de versions del fitxer, i el seu nom sense .md */
static char *versions_dir(const Project *p, const FNode *n)
{
    char *a = join(p->root, ".versions"), *b = join(a, n->parent ? n->parent->name : "");

    free(a);
    return b;
}

static char *base_name(const FNode *n)
{
    size_t l = strlen(n->name);
    char *b = xstrdup(n->name);

    if (l > 3 && !strcmp(b + l - 3, ".md"))
        b[l - 3] = '\0';
    return b;
}

static int cmp_ver_desc(const void *a, const void *b)
{
    return strcmp(((const FVersion *)b)->stamp, ((const FVersion *)a)->stamp);
}

size_t project_versions(const Project *p, const FNode *n, FVersion **out)
{
    char *dir, *base;
    DIR *d;
    struct dirent *e;
    FVersion *v = NULL;
    size_t cnt = 0, cap = 0, bl;

    *out = NULL;
    if ((n->kind != FK_CHAPTER && n->kind != FK_CARD) || !n->parent)
        return 0;
    dir = versions_dir(p, n);
    base = base_name(n);
    bl = strlen(base);
    d = opendir(dir);
    if (d) {
        while ((e = readdir(d))) {
            size_t l = strlen(e->d_name);
            time_t t;

            if (l != bl + 1 + STAMP_LEN + 3 || strncmp(e->d_name, base, bl) || e->d_name[bl] != '.' ||
                strcmp(e->d_name + l - 3, ".md") || !parse_stamp(e->d_name + bl + 1, &t))
                continue;
            if (cnt == cap)
                v = xrealloc(v, (cap = cap ? cap * 2 : 8) * sizeof *v);
            memset(&v[cnt], 0, sizeof v[cnt]);
            memcpy(v[cnt].stamp, e->d_name + bl + 1, STAMP_LEN);
            v[cnt].stamp[STAMP_LEN] = '\0';
            v[cnt].when = t;
            v[cnt].path = join(dir, e->d_name);
            cnt++;
        }
        closedir(d);
    }
    free(dir);
    free(base);
    if (cnt)
        qsort(v, cnt, sizeof *v, cmp_ver_desc);
    for (size_t i = 0; i < cnt; i++) {
        struct stat st;
        Buffer b;

        if (stat(v[i].path, &st) == 0)
            v[i].size = st.st_size;
        buf_init(&b);
        if (buf_load(&b, v[i].path) == 0)
            v[i].words = prose_words(&b);
        buf_free(&b);
    }
    *out = v;
    return cnt;
}

void project_versions_free(FVersion *v, size_t n)
{
    for (size_t i = 0; i < n; i++)
        free(v[i].path);
    free(v);
}

static int mkdir_p1(const char *path)
{
    return mkdir(path, 0777) != 0 && errno != EEXIST ? -1 : 0;
}

int project_snapshot(Project *p, FNode *n, int force, time_t now, char *err, size_t esz)
{
    FVersion *v;
    size_t cnt, len = 0;
    char *cur, *dir, *base, stamp[16], *dst, name[400], *vdata;
    struct stat st;
    int rc = -1;

    if ((n->kind != FK_CHAPTER && n->kind != FK_CARD) || !n->path || !n->parent) {
        snprintf(err, esz, "Només es fan instantànies de capítols i fitxes.");
        return -1;
    }
    cur = read_all(n->path, &len);
    if (!cur) {
        fail(err, esz, "No s'ha pogut llegir el fitxer");
        return -1;
    }
    cnt = project_versions(p, n, &v);
    if (cnt) {
        size_t vl = 0;

        vdata = read_all(v[0].path, &vl);
        if (vdata && vl == len && memcmp(vdata, cur, len) == 0) {
            free(vdata);
            free(cur);
            project_versions_free(v, cnt);
            return 1;                                   /* idèntica a la darrera */
        }
        free(vdata);
        if (!force && now - v[0].when < FK_SNAPSHOT_GAP && now >= v[0].when) {
            free(cur);
            project_versions_free(v, cnt);
            return 1;
        }
    }
    {
        char *root = join(p->root, ".versions");

        if (mkdir_p1(root) != 0) {
            fail(err, esz, "No s'ha pogut crear .versions/");
            free(root);
            goto out;
        }
        free(root);
    }
    dir = versions_dir(p, n);
    if (mkdir_p1(dir) != 0) {
        fail(err, esz, "No s'ha pogut crear la carpeta de versions");
        free(dir);
        goto out;
    }
    base = base_name(n);
    make_stamp(now, stamp);
    snprintf(name, sizeof name, "%s.%s.md", base, stamp);
    dst = join(dir, name);
    free(base);
    free(dir);
    if (exists(dst)) {
        rc = 1;                                          /* ja n'hi ha una d'aquest segon */
        free(dst);
        goto out;
    }
    if (write_atomic(dst, cur, len) != 0) {
        fail(err, esz, "No s'ha pogut desar la instantània");
        free(dst);
        goto out;
    }
    if (stat(n->path, &st) == 0)
        (void)chmod(dst, st.st_mode & 07777);
    free(dst);
    rc = 0;

    /* poda: les més antigues passen de FK_VERSIONS_MAX van a .paperera/ */
    project_versions_free(v, cnt);
    cnt = project_versions(p, n, &v);
    for (size_t i = FK_VERSIONS_MAX; i < cnt; i++) {
        char e2[200], vn[400];
        const char *slash = strrchr(v[i].path, '/');

        snprintf(vn, sizeof vn, "versio-%s", slash ? slash + 1 : v[i].path);
        (void)trash_path(p->root, v[i].path, vn, e2, sizeof e2);
    }
out:
    free(cur);
    project_versions_free(v, cnt);
    return rc;
}

int project_restore(Project *p, FNode *n, const FVersion *ver, time_t now, char *err, size_t esz)
{
    size_t len = 0;
    char *data;
    int rc, s;

    s = project_snapshot(p, n, 1, now, err, esz);          /* l'actual, per poder tornar enrere */
    if (s < 0)
        return -1;
    data = read_all(ver->path, &len);
    if (!data) {
        fail(err, esz, "No s'ha pogut llegir la versió");
        return -1;
    }
    rc = write_atomic(n->path, data, len);
    free(data);
    if (rc != 0) {
        fail(err, esz, "No s'ha pogut recuperar la versió");
        return -1;
    }
    return 0;
}

/* ---------- edició de metadades ---------- */

static int valid_date(const char *s)
{
    size_t l = strlen(s);

    if (l == 0)
        return 1;
    if (l != 4 && l != 7 && l != 10)
        return 0;
    for (size_t i = 0; i < l; i++)
        if (i == 4 || i == 7 ? s[i] != '-' : !isdigit((unsigned char)s[i]))
            return 0;
    if (l >= 7 && (atoi(s + 5) < 1 || atoi(s + 5) > 12))
        return 0;
    if (l == 10 && (atoi(s + 8) < 1 || atoi(s + 8) > 31))
        return 0;
    return 1;
}

/* Valor net per a un comentari: una línia, sense «;» ni «-->» */
static void comment_value(const char *in, char *out, size_t osz)
{
    size_t o = 0;

    while (*in && isspace((unsigned char)*in))
        in++;
    for (; *in && o + 1 < osz; in++) {
        if (*in == '\n' || *in == '\r' || *in == '\t')
            out[o++] = ' ';
        else if (*in == ';')
            out[o++] = ',';
        else if (!strncmp(in, "-->", 3))
            in += 2;
        else
            out[o++] = *in;
    }
    while (o && out[o - 1] == ' ')
        o--;
    out[o] = '\0';
}

static int is_fence_line(const char *s, size_t len)
{
    size_t i = 0;

    while (i < len && i < 3 && s[i] == ' ')
        i++;
    return len - i >= 3 && (!strncmp(s + i, "```", 3) || !strncmp(s + i, "~~~", 3));
}

/* Posa o treu un camp del comentari de l'escena `ordinal` (la n-èsima secció ###) */
static char *scene_edit(const char *text, size_t len, size_t ordinal, const char *title,
                        const char *key, const char *value, size_t *outlen, char *err, size_t esz)
{
    size_t pos = 0, count = 0, hs = (size_t)-1, he = 0;
    int fence = 0;

    /* salta la capçalera YAML */
    if (len >= 4 && !strncmp(text, "---", 3) && (text[3] == '\n' || text[3] == '\r')) {
        size_t i = 3;

        while (i < len && text[i] != '\n')
            i++;
        i++;
        while (i < len) {
            size_t e = i;

            while (e < len && text[e] != '\n')
                e++;
            if ((e - i == 3 || (e - i == 4 && text[e - 1] == '\r')) &&
                (!strncmp(text + i, "---", 3) || !strncmp(text + i, "...", 3))) {
                pos = e < len ? e + 1 : e;
                break;
            }
            i = e < len ? e + 1 : e;
        }
    }
    while (pos < len) {
        size_t e = pos, sp = 0;

        while (e < len && text[e] != '\n')
            e++;
        if (is_fence_line(text + pos, e - pos)) {
            fence = !fence;
        } else if (!fence) {
            while (pos + sp < e && sp < 3 && text[pos + sp] == ' ')
                sp++;
            if (e - pos - sp >= 3 && !strncmp(text + pos + sp, "###", 3) &&
                (e - pos - sp == 3 || text[pos + sp + 3] == ' ' || text[pos + sp + 3] == '\t')) {
                if (count++ == ordinal) {
                    hs = pos;
                    he = e;
                    break;
                }
            }
        }
        pos = e < len ? e + 1 : e;
    }
    if (hs == (size_t)-1) {
        snprintf(err, esz, "No trobo l'escena al fitxer (ha canviat?); torna a llegir el projecte.");
        return NULL;
    }
    {
        /* comprova el títol */
        size_t a = hs, z = he;
        char heading[300];

        while (a < z && text[a] == ' ')
            a++;
        a += 3;
        while (a < z && (text[a] == ' ' || text[a] == '\t'))
            a++;
        while (z > a && (text[z - 1] == '\r' || text[z - 1] == ' ' || text[z - 1] == '#' || text[z - 1] == '\t'))
            z--;
        snprintf(heading, sizeof heading, "%.*s", (int)(z - a), text + a);
        if (strcmp(heading, title)) {
            snprintf(err, esz, "L'escena «%s» ha canviat al fitxer; torna a llegir el projecte.", title);
            return NULL;
        }
    }
    {
        int crlf = he > hs && text[he - 1] == '\r';
        const char *eol = crlf ? "\r\n" : "\n";
        size_t hend = he < len ? he + 1 : he;           /* després del salt de la capçalera */
        size_t ne = hend, ce = hend;
        Fields f;
        char line[1024], cv[512], *res;
        int have = 0, found = 0;
        size_t o = 0;

        memset(&f, 0, sizeof f);
        if (hend < len) {
            while (ne < len && text[ne] != '\n')
                ne++;
            fields_parse(text + hend, ne - hend, &f);
            if (f.n > 0) {
                have = 1;
                ce = ne < len ? ne + 1 : ne;
            }
        }
        comment_value(value, cv, sizeof cv);
        /* reconstrueix: es conserva l'ordre i els camps que no entenem */
        o = (size_t)snprintf(line, sizeof line, "<!--");
        for (int i = 0; i < f.n; i++) {
            if (!strcmp(f.key[i], key)) {
                found = 1;
                if (!cv[0])
                    continue;
                o += (size_t)snprintf(line + o, sizeof line - o, "%s %s: %s", o > 4 ? ";" : "", key, cv);
            } else {
                o += (size_t)snprintf(line + o, sizeof line - o, "%s %s: %s", o > 4 ? ";" : "", f.key[i], f.val[i]);
            }
            if (o >= sizeof line - 8)
                break;
        }
        if (!found && cv[0])
            o += (size_t)snprintf(line + o, sizeof line - o, "%s %s: %s", o > 4 ? ";" : "", key, cv);
        fields_free(&f);
        res = xmalloc(len + sizeof line + 16);
        memcpy(res, text, hend);
        o = hend;
        if (he >= len)                                  /* el fitxer acaba a la capçalera */
            o += (size_t)sprintf(res + o, "%s", eol);
        {
            size_t ll = strlen(line);

            if (ll > 4) {                               /* queda algun camp */
                memcpy(res + o, line, ll);
                o += ll;
                o += (size_t)sprintf(res + o, " -->%s", eol);
            }
        }
        {
            size_t rest = have ? ce : hend;

            memcpy(res + o, text + rest, len - rest);
            o += len - rest;
        }
        res[o] = '\0';
        *outlen = o;
        return res;
    }
}

int project_set_meta(Project *p, FNode *n, const char *key, const char *value, char *err, size_t esz)
{
    static const char *const keys[] = { "pov", "lloc", "data", "personatges", "sinopsi", "estat" };
    char clean[512], *text, *res;
    size_t len, rl;
    int ok = 0, rc;

    (void)p;
    for (size_t i = 0; i < sizeof keys / sizeof *keys; i++)
        ok |= !strcmp(key, keys[i]);
    if (!ok) {
        snprintf(err, esz, "Camp desconegut: %s", key);
        return -1;
    }
    if (n->kind != FK_CHAPTER && n->kind != FK_CARD && n->kind != FK_SCENE) {
        snprintf(err, esz, "Les metadades són de capítols, escenes i fitxes.");
        return -1;
    }
    if (n->kind == FK_SCENE && !n->parent) {
        snprintf(err, esz, "Escena sense capítol.");
        return -1;
    }
    {
        size_t o = 0;
        const char *s = value;

        while (*s && isspace((unsigned char)*s))
            s++;
        for (; *s && o + 1 < sizeof clean; s++)
            clean[o++] = (*s == '\n' || *s == '\r' || *s == '\t') ? ' ' : *s;
        while (o && clean[o - 1] == ' ')
            o--;
        clean[o] = '\0';
    }
    if (!strcmp(key, "data") && !valid_date(clean)) {
        snprintf(err, esz, "Data no vàlida: ha de ser AAAA, AAAA-MM o AAAA-MM-DD.");
        return -1;
    }
    {
        const char *path = n->kind == FK_SCENE ? n->parent->path : n->path;

        text = read_all(path, &len);
        if (!text) {
            fail(err, esz, "No s'ha pogut llegir el fitxer");
            return -1;
        }
        if (n->kind == FK_SCENE) {
            size_t ord = 0;

            for (size_t i = 0; i < n->parent->nkids; i++)
                if (n->parent->kids[i] == n)
                    ord = i;
            res = scene_edit(text, len, ord, n->title, key, clean, &rl, err, esz);
            free(text);
            if (!res)
                return -1;
        } else {
            res = yaml_set(text, len, key, clean, 0, &rl);
            free(text);
        }
        rc = write_atomic(path, res, rl);
        free(res);
        if (rc != 0) {
            fail(err, esz, "No s'ha pogut desar");
            return -1;
        }
    }
    return 0;
}


/* ---------- consultes d'enllaços ---------- */

size_t project_backlinks(const Project *p, const FNode *target, FBack **out)
{
    FBack *v = NULL;
    size_t n = 0, cap = 0;

    *out = NULL;
    if (!p->top)
        return 0;
    /* primer el manuscrit, després la resta de grups */
    for (int pass = 0; pass < 2; pass++)
        for (size_t g = 0; g < p->top->nkids; g++) {
            const FNode *grp = p->top->kids[g];

            if ((pass == 0) != (grp == p->manuscrit))
                continue;
            for (size_t i = 0; i < grp->nkids; i++) {
                FNode *f = grp->kids[i];

                if (f == target)
                    continue;
                for (size_t k = 0; k < f->nlinks; k++)
                    if (f->links[k].to == target) {
                        if (n == cap)
                            v = xrealloc(v, (cap = cap ? cap * 2 : 16) * sizeof *v);
                        v[n].file = f;
                        v[n++].link = &f->links[k];
                    }
            }
        }
    *out = v;
    return n;
}

size_t project_broken_links(const Project *p, FBack **out)
{
    FBack *v = NULL;
    size_t n = 0, cap = 0;

    *out = NULL;
    if (!p->top)
        return 0;
    for (int pass = 0; pass < 2; pass++)
        for (size_t g = 0; g < p->top->nkids; g++) {
            const FNode *grp = p->top->kids[g];

            if ((pass == 0) != (grp == p->manuscrit))
                continue;
            for (size_t i = 0; i < grp->nkids; i++) {
                FNode *f = grp->kids[i];

                for (size_t k = 0; k < f->nlinks; k++)
                    if (!f->links[k].to) {
                        if (n == cap)
                            v = xrealloc(v, (cap = cap ? cap * 2 : 16) * sizeof *v);
                        v[n].file = f;
                        v[n++].link = &f->links[k];
                    }
            }
        }
    *out = v;
    return n;
}
