#include "rview.h"

#include "fold.h"
#include "front.h"
#include "fterm.h"
#include "launch.h"
#include "ui.h"
#include "mdscan.h"
#include "model.h"
#include "nova.h"
#include "retalls.h"

#include "clipboard.h"

#include "keys.h"
#include "utf8.h"
#include "util.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

enum { F_NAV, F_LIST, F_DOC, F_BACK };

/* ---------- document del visor ---------- */

typedef struct {
    size_t line;            /* línia del fitxer (1 = la primera) */
    size_t start, len;      /* marca sencera, en bytes dins de la línia */
    char  *target;          /* destí, sense secció ni text */
    char  *section;         /* NULL, o la secció o el bloc */
    int    block;
} DLink;

typedef enum { R_PROP, R_RULE, R_BODY, R_BLANK } RowKind;

typedef struct {
    RowKind kind;
    size_t  idx;            /* R_PROP: propietat; R_BODY: línia del fitxer (0 = la primera) */
    size_t  b, e;           /* R_BODY: tros de la línia (bytes) */
} DRow;

typedef struct {
    long long id;
    char     *buf;
    size_t    n;
    size_t   *lo, *ll;      /* inici i llargada de cada línia */
    size_t    nlines;
    size_t    body;         /* primera línia del cos (0 = la primera del fitxer) */
    char    **props;        /* «clau: valor» per a la capçalera */
    size_t    nprops;
    DLink    *links;
    size_t    nlinks;
    DRow     *rows;
    size_t    nrows;
    int       width;        /* amplada amb què s'han fet les files */
    long      top;
    long      link;         /* enllaç seleccionat, -1 = cap */
} Doc;

typedef struct {
    long long id;
    long      top, link;
} Hist;

typedef struct {
    long long de;
    char     *nom, *cami, *context, *relacio;
    int       linia;
} Back;

typedef struct {
    Vault   *v;
    Model    m;
    int      focus;
    long     nav_sel, nav_top;
    size_t  *list;
    size_t   nlist;
    long     list_sel, list_top;
    char     filter[128];
    int      editing;
    SortMode sort;
    Doc      doc;
    int      maxi;          /* el visor ocupa tota la pantalla */
    int      show_back;
    Back    *back;
    size_t   nback;
    long     back_sel, back_top;
    long long back_of;      /* nota de la qual són els backlinks */
    Hist     hist[64];
    int      nhist;
    char     msg[4400];
    RviewOpts opt;
    int      marking;       /* «m»: paràgrafs marcats al visor per copiar-los */
    size_t   mark_a, mark_b; /* primera i darrera línia marcades (del fitxer, 0 = la primera) */
    long long hint_id;      /* fitxa i línia on ha portat la cerca: «m» hi comença */
    size_t   hint_line;
} RView;

static void sep_line(int x, int y0, int y1)
{
    attron(ft_attr(FT_DIM));
    for (int y = y0; y <= y1; y++)
        mvaddstr(y, x, "│");
    attroff(ft_attr(FT_DIM));
}

/* ---------- document ---------- */

static void doc_free(Doc *d)
{
    for (size_t i = 0; i < d->nprops; i++)
        free(d->props[i]);
    for (size_t i = 0; i < d->nlinks; i++) {
        free(d->links[i].target);
        free(d->links[i].section);
    }
    free(d->props);
    free(d->links);
    free(d->rows);
    free(d->lo);
    free(d->ll);
    free(d->buf);
    memset(d, 0, sizeof *d);
    d->link = -1;
}

static void prop_cb(const char *k, size_t kl, const char *v, size_t vl, int idx, void *ud)
{
    Doc *d = ud;
    char *s;

    if (idx > 0 && d->nprops) {             /* un altre element de la mateixa llista */
        char *p = d->props[d->nprops - 1];
        size_t pl = strlen(p);

        p = xrealloc(p, pl + vl + 3);
        memcpy(p + pl, ", ", 2);
        memcpy(p + pl + 2, v, vl);
        p[pl + 2 + vl] = '\0';
        d->props[d->nprops - 1] = p;
        return;
    }
    s = xmalloc(kl + vl + 3);
    memcpy(s, k, kl);
    memcpy(s + kl, ": ", 2);
    memcpy(s + kl + 2, v, vl);
    s[kl + 2 + vl] = '\0';
    d->props = xrealloc(d->props, (d->nprops + 1) * sizeof *d->props);
    d->props[d->nprops++] = s;
}

static void link_cb(const CmRef *r, void *ud)
{
    Doc *d = ud;
    DLink *l;

    if (r->kind != CM_LINK)
        return;
    d->links = xrealloc(d->links, (d->nlinks + 1) * sizeof *d->links);
    l = &d->links[d->nlinks++];
    l->line = r->line;
    l->start = r->start;
    l->len = r->len;
    l->target = xmalloc(r->target_len + 1);
    memcpy(l->target, r->target, r->target_len);
    l->target[r->target_len] = '\0';
    l->section = NULL;
    if (r->section) {
        l->section = xmalloc(r->section_len + 1);
        memcpy(l->section, r->section, r->section_len);
        l->section[r->section_len] = '\0';
    }
    l->block = r->block;
}

static int doc_load(RView *rv, long long id)
{
    Doc *d = &rv->doc;
    long i = model_find(&rv->m, id);
    char path[8192];
    FILE *f;
    size_t off, body_line;

    doc_free(d);
    if (i < 0)
        return -1;
    snprintf(path, sizeof path, "%s/%s", vault_root(rv->v), rv->m.notes[i].cami);
    f = fopen(path, "rb");
    if (!f) {
        snprintf(rv->msg, sizeof rv->msg, "No es pot llegir %s", rv->m.notes[i].cami);
        return -1;
    }
    {
        size_t cap = 16384, n = 0, k;

        d->buf = xmalloc(cap + 1);
        while ((k = fread(d->buf + n, 1, cap - n, f)) > 0) {
            n += k;
            if (n == cap) {
                cap *= 2;
                d->buf = xrealloc(d->buf, cap + 1);
            }
        }
        d->buf[n] = '\0';
        d->n = n;
    }
    fclose(f);
    d->id = id;
    /* línies */
    {
        size_t cap = 256;

        d->lo = xmalloc(cap * sizeof *d->lo);
        d->ll = xmalloc(cap * sizeof *d->ll);
        for (size_t p = 0; p <= d->n;) {
            const char *e = memchr(d->buf + p, '\n', d->n - p);
            size_t len = (e ? (size_t)(e - d->buf) : d->n) - p;

            if (d->nlines == cap) {
                cap *= 2;
                d->lo = xrealloc(d->lo, cap * sizeof *d->lo);
                d->ll = xrealloc(d->ll, cap * sizeof *d->ll);
            }
            if (len && d->buf[p + len - 1] == '\r')
                len--;
            d->lo[d->nlines] = p;
            d->ll[d->nlines++] = len;
            if (!e)
                break;
            p = (size_t)(e - d->buf) + 1;
        }
    }
    off = cm_front(d->buf, d->n, &body_line, prop_cb, d);
    d->body = off ? body_line - 1 : 0;
    cm_scan(d->buf + off, d->n - off, body_line, link_cb, d);
    d->top = 0;
    d->link = -1;
    d->width = 0;
    return 0;
}

static void add_row(Doc *d, RowKind k, size_t idx, size_t b, size_t e)
{
    if (!(d->nrows & 255))
        d->rows = xrealloc(d->rows, (d->nrows + 256) * sizeof *d->rows);
    d->rows[d->nrows++] = (DRow){ k, idx, b, e };
}

/* Parteix cada línia en files de com a molt w columnes, pels espais */
static void doc_wrap(Doc *d, int w)
{
    if (w < 10)
        w = 10;
    free(d->rows);
    d->rows = NULL;
    d->nrows = 0;
    d->width = w;
    for (size_t i = 0; i < d->nprops; i++)
        add_row(d, R_PROP, i, 0, 0);
    if (d->nprops)
        add_row(d, R_RULE, 0, 0, 0);
    for (size_t l = d->body; l < d->nlines; l++) {
        const char *s = d->buf + d->lo[l];
        size_t n = d->ll[l], b = 0;

        if (!n) {
            add_row(d, R_BLANK, l, 0, 0);
            continue;
        }
        while (b < n) {
            size_t i = b, brk = 0;
            int col = 0;

            while (i < n) {
                wchar_t c;
                size_t k = utf8_decode(s, n, i, &c);
                int cw = c == UTF8_INVALID ? 1 : c == '\t' ? 4 : utf8_width(c, col);

                if (cw < 0)
                    cw = 1;
                if (col + cw > w)
                    break;
                col += cw;
                i += k ? k : 1;
                if (c == ' ')
                    brk = i;
            }
            if (i < n && brk > b)
                i = brk;
            if (i == b)                 /* un caràcter més ample que la fila */
                i = utf8_next(s, n, b);
            add_row(d, R_BODY, l, b, i);
            b = i;
        }
    }
}

/* Primera fila de la línia del fitxer l */
static long row_of_line(const Doc *d, size_t l)
{
    for (size_t r = 0; r < d->nrows; r++)
        if ((d->rows[r].kind == R_BODY || d->rows[r].kind == R_BLANK) && d->rows[r].idx >= l)
            return (long)r;
    return 0;
}

static long row_of_link(const Doc *d, long k)
{
    const DLink *l = &d->links[k];

    for (size_t r = 0; r < d->nrows; r++)
        if (d->rows[r].kind == R_BODY && d->rows[r].idx == l->line - 1 && l->start < d->rows[r].e)
            return (long)r;
    return 0;
}

/* Línia on és la capçalera «section» o el bloc «^section» */
static long find_section(const Doc *d, const char *section, int block)
{
    char *want = cm_lower(section, strlen(section));
    long found = -1;

    for (size_t l = d->body; l < d->nlines && found < 0; l++) {
        const char *s = d->buf + d->lo[l];
        size_t n = d->ll[l];

        if (block) {
            size_t k = strlen(section);

            while (n && (s[n - 1] == ' ' || s[n - 1] == '\t'))
                n--;
            if (n > k && s[n - k - 1] == '^' && !strncmp(s + n - k, section, k))
                found = (long)l;
        } else if (n && s[0] == '#') {
            size_t h = 0;
            char *got;

            while (h < n && s[h] == '#')
                h++;
            while (h < n && s[h] == ' ')
                h++;
            got = cm_lower(s + h, n - h);
            if (!strcmp(got, want))
                found = (long)l;
            free(got);
        }
    }
    free(want);
    return found;
}

/* ---------- backlinks ---------- */

static void back_free(RView *rv)
{
    for (size_t i = 0; i < rv->nback; i++) {
        free(rv->back[i].nom);
        free(rv->back[i].cami);
        free(rv->back[i].context);
        free(rv->back[i].relacio);
    }
    free(rv->back);
    rv->back = NULL;
    rv->nback = 0;
    rv->back_sel = rv->back_top = 0;
    rv->back_of = 0;
}

static char *dupcol(sqlite3_stmt *s, int i)
{
    const char *t = (const char *)sqlite3_column_text(s, i);

    return t ? xstrdup(t) : NULL;
}

static void back_load(RView *rv, long long id)
{
    sqlite3_stmt *s;

    back_free(rv);
    rv->back_of = id;
    if (sqlite3_prepare_v2(vault_db(rv->v),
            "SELECT e.de, f.nom, f.cami, e.linia, e.context, e.relacio FROM enllacos e "
            "JOIN fitxes f ON f.id = e.de WHERE e.a = ? AND e.de <> e.a ORDER BY f.nom, e.linia",
            -1, &s, NULL) != SQLITE_OK)
        return;
    sqlite3_bind_int64(s, 1, id);
    while (sqlite3_step(s) == SQLITE_ROW) {
        Back *b;

        rv->back = xrealloc(rv->back, (rv->nback + 1) * sizeof *rv->back);
        b = &rv->back[rv->nback++];
        b->de = sqlite3_column_int64(s, 0);
        b->nom = dupcol(s, 1);
        b->cami = dupcol(s, 2);
        b->linia = sqlite3_column_int(s, 3);
        b->context = dupcol(s, 4);
        b->relacio = dupcol(s, 5);
    }
    sqlite3_finalize(s);
}

/* ---------- disposició ---------- */

typedef struct {
    int nav_x, nav_w;       /* w = 0: no es veu */
    int list_x, list_w;
    int doc_x, doc_w;
    int y0, y1;             /* files dels panells (incloent-hi el títol) */
    int doc_y1;             /* última fila del text del visor */
    int back_y0;            /* primera fila del panell de backlinks, o -1 */
} Lay;

static Lay layout(const RView *rv)
{
    Lay l;
    int left;

    memset(&l, 0, sizeof l);
    l.y0 = 1;
    l.y1 = LINES - 2;
    if (rv->maxi) {
        l.doc_x = 0;
        l.doc_w = COLS;
    } else if (COLS >= 110) {
        l.nav_w = 24;
        l.list_x = l.nav_w + 1;
        l.list_w = 36;
        l.doc_x = l.list_x + l.list_w + 1;
        l.doc_w = COLS - l.doc_x;
    } else {
        left = COLS >= 90 ? 34 : COLS >= 70 ? 30 : COLS / 2;
        if (rv->focus == F_NAV) {
            l.nav_w = left;
        } else {
            l.list_w = left;
        }
        l.doc_x = left + 1;
        l.doc_w = COLS - l.doc_x;
    }
    l.doc_y1 = l.y1;
    l.back_y0 = -1;
    if (rv->show_back) {
        int h = (l.y1 - l.y0 + 1) / 3;

        if (h < 5)
            h = 5;
        l.back_y0 = l.y1 - h + 1;
        l.doc_y1 = l.back_y0 - 1;
    }
    return l;
}

static const MNav *cur_nav(const RView *rv)
{
    return rv->nav_sel >= 0 && (size_t)rv->nav_sel < rv->m.nnav ? &rv->m.nav[rv->nav_sel] : NULL;
}

static const MNote *cur_note(const RView *rv)
{
    return rv->nlist && rv->list_sel >= 0 && (size_t)rv->list_sel < rv->nlist
           ? &rv->m.notes[rv->list[rv->list_sel]] : NULL;
}

static void scroll_into(long *top, long sel, int rows)
{
    if (rows < 1)
        rows = 1;
    if (sel < *top)
        *top = sel;
    if (sel >= *top + rows)
        *top = sel - rows + 1;
    if (*top < 0)
        *top = 0;
}

/* ---------- panells ---------- */

static void pane_title(int y, int x, int w, const char *s, int focused)
{
    cm_blank(y, x, w, focused ? ft_attr(FT_SEL) : 0);
    cm_put(y, x + 1, s, w - 1, focused ? ft_attr(FT_SEL) : ft_attr(FT_HEAD));
}

static void paint_nav(RView *rv, const Lay *l)
{
    int rows = l->y1 - l->y0;
    char t[64];

    snprintf(t, sizeof t, "%s  (1 2 3)", model_mode_label(rv->m.mode));
    pane_title(l->y0, l->nav_x, l->nav_w, t, rv->focus == F_NAV);
    scroll_into(&rv->nav_top, rv->nav_sel, rows);
    for (int r = 0; r < rows; r++) {
        long i = rv->nav_top + r;
        int y = l->y0 + 1 + r;
        attr_t a;
        char cnt[32];
        int ind, cw;

        cm_blank(y, l->nav_x, l->nav_w, 0);
        if (i >= (long)rv->m.nnav)
            continue;
        a = i == rv->nav_sel ? (rv->focus == F_NAV ? ft_attr(FT_SEL) : A_BOLD) : 0;
        if (a)
            cm_blank(y, l->nav_x, l->nav_w, a);
        vault_fmt_num(cnt, sizeof cnt, (long long)rv->m.nav[i].count);
        cw = cm_width(cnt);
        ind = 1 + 2 * (rv->m.nav[i].depth > 0 ? rv->m.nav[i].depth - 1 : 0);
        if (ind > l->nav_w / 2)
            ind = l->nav_w / 2;
        cm_put(y, l->nav_x + ind, rv->m.nav[i].label, l->nav_w - ind - cw - 2, a);
        cm_put(y, l->nav_x + l->nav_w - cw - 1, cnt, cw, a ? a : ft_attr(FT_DIM));
    }
}

static void paint_list(RView *rv, const Lay *l)
{
    int rows = l->y1 - l->y0;
    char t[200], cnt[32];

    vault_fmt_num(cnt, sizeof cnt, (long long)rv->nlist);
    if (rv->editing || rv->filter[0])
        snprintf(t, sizeof t, "/%s%s  %s", rv->filter, rv->editing ? "▏" : "", cnt);
    else
        snprintf(t, sizeof t, "Fitxes %s · %s", cnt, model_sort_label(rv->sort));
    pane_title(l->y0, l->list_x, l->list_w, t, rv->focus == F_LIST);
    scroll_into(&rv->list_top, rv->list_sel, rows);
    for (int r = 0; r < rows; r++) {
        long i = rv->list_top + r;
        int y = l->y0 + 1 + r, dw;
        const MNote *n;
        attr_t a;
        char lt[2];

        cm_blank(y, l->list_x, l->list_w, 0);
        if (i >= (long)rv->nlist)
            continue;
        n = &rv->m.notes[rv->list[i]];
        a = i == rv->list_sel ? (rv->focus == F_LIST ? ft_attr(FT_SEL) : A_BOLD) : 0;
        if (a)
            cm_blank(y, l->list_x, l->list_w, a);
        lt[0] = model_type_letter(n->tipus);
        lt[1] = '\0';
        cm_put(y, l->list_x + 1, lt, 1, a ? a : ft_attr(FT_DIM));
        dw = l->list_w > 30 ? 10 : 0;          /* columna fixa: AAAA-MM-DD */
        cm_put(y, l->list_x + 3, n->nom, l->list_w - 4 - (dw ? dw + 1 : 0), a);
        if (dw && n->data[0]) {
            int tw = cm_width(n->data);

            if (tw > dw)
                tw = dw;
            cm_put(y, l->list_x + l->list_w - tw - 1, n->data, tw, a ? a : ft_attr(FT_DIM));
        }
    }
}

/* Dibuixa una fila del cos amb els enllaços subratllats (el seleccionat,
 * en vídeo invers) i les capçaleres destacades */
static void paint_body_row(const RView *rv, const DRow *row, int y, int x, int w)
{
    const Doc *d = &rv->doc;
    const char *s = d->buf + d->lo[row->idx];
    size_t n = d->ll[row->idx];
    int head = n && s[0] == '#', col = 0;
    attr_t base = head ? ft_attr(FT_HEAD) : 0;
    size_t lo = rv->mark_a < rv->mark_b ? rv->mark_a : rv->mark_b;
    size_t hi = rv->mark_a < rv->mark_b ? rv->mark_b : rv->mark_a;

    if (rv->marking && row->idx >= lo && row->idx <= hi) {   /* marcat: fins al final de la fila */
        base = ft_attr(FT_SEL);
        cm_blank(y, x, w, base);
    }

    move(y, x);
    for (size_t i = row->b; i < row->e && col < w;) {
        wchar_t c;
        size_t k = utf8_decode(s, n, i, &c);
        int cw = c == UTF8_INVALID ? 1 : c == '\t' ? 4 : utf8_width(c, col);
        attr_t a = base;

        if (cw < 0)
            cw = 1;
        for (size_t j = 0; j < d->nlinks; j++) {
            const DLink *lk = &d->links[j];

            if (lk->line - 1 == row->idx && i >= lk->start && i < lk->start + lk->len) {
                a = (long)j == d->link ? ft_attr(FT_SEL) : (base | A_UNDERLINE);
                break;
            }
        }
        if (col + cw > w)
            break;
        attron(a);
        if (c == UTF8_INVALID || !utf8_printable(c))
            addch(c == '\t' ? ' ' : '?');
        else
            addnstr(s + i, (int)(k ? k : 1));
        if (c == '\t')
            for (int t = 1; t < 4; t++)
                addch(' ');
        attroff(a);
        col += cw;
        i += k ? k : 1;
    }
}

static void paint_doc(RView *rv, const Lay *l)
{
    Doc *d = &rv->doc;
    int rows = l->doc_y1 - l->y0, w = l->doc_w - 2;
    long i = model_find(&rv->m, d->id);
    char t[600];

    if (i >= 0)
        snprintf(t, sizeof t, "%s%s", rv->nhist ? "← " : "", rv->m.notes[i].cami);
    else
        snprintf(t, sizeof t, "(cap fitxa)");
    pane_title(l->y0, l->doc_x, l->doc_w, t, rv->focus == F_DOC);
    if (d->buf && d->width != w)
        doc_wrap(d, w);
    if (d->top > (long)d->nrows - rows)
        d->top = (long)d->nrows - rows;
    if (d->top < 0)
        d->top = 0;
    for (int r = 0; r < rows; r++) {
        long k = d->top + r;
        int y = l->y0 + 1 + r;

        cm_blank(y, l->doc_x, l->doc_w, 0);
        if (!d->buf || k >= (long)d->nrows)
            continue;
        switch (d->rows[k].kind) {
        case R_PROP:
            cm_put(y, l->doc_x + 1, d->props[d->rows[k].idx], w, ft_attr(FT_DIM));
            break;
        case R_RULE:
            attron(ft_attr(FT_DIM));
            move(y, l->doc_x + 1);
            for (int c = 0; c < w && c < 40; c++)
                addstr("╌");
            attroff(ft_attr(FT_DIM));
            break;
        case R_BODY:
            paint_body_row(rv, &d->rows[k], y, l->doc_x + 1, w);
            break;
        default:
            break;
        }
    }
    if (d->buf && d->nrows > (size_t)rows) {        /* posició */
        char pos[32];

        snprintf(pos, sizeof pos, " %ld%% ", (long)(100 * (d->top + rows) / (long)d->nrows) > 100 ? 100 :
                 (long)(100 * (d->top + rows) / (long)d->nrows));
        cm_put(l->doc_y1, l->doc_x + l->doc_w - cm_width(pos) - 1, pos, cm_width(pos), ft_attr(FT_DIM));
    }
}

static void paint_back(RView *rv, const Lay *l)
{
    int y0 = l->back_y0, rows = l->y1 - y0;
    char t[64], cnt[32];

    if (rv->back_of != rv->doc.id)
        back_load(rv, rv->doc.id);
    vault_fmt_num(cnt, sizeof cnt, (long long)rv->nback);
    snprintf(t, sizeof t, "Hi enllacen (%s)", cnt);
    pane_title(y0, l->doc_x, l->doc_w, t, rv->focus == F_BACK);
    scroll_into(&rv->back_top, rv->back_sel, rows);
    for (int r = 0; r < rows; r++) {
        long i = rv->back_top + r;
        int y = y0 + 1 + r, x;
        const Back *b;
        attr_t a;
        char head[300];

        cm_blank(y, l->doc_x, l->doc_w, 0);
        if (i >= (long)rv->nback) {
            if (!rv->nback && r == 0)
                cm_put(y, l->doc_x + 1, "Cap nota no hi enllaça.", l->doc_w - 2, ft_attr(FT_DIM));
            continue;
        }
        b = &rv->back[i];
        a = i == rv->back_sel && rv->focus == F_BACK ? ft_attr(FT_SEL) : 0;
        if (a)
            cm_blank(y, l->doc_x, l->doc_w, a);
        if (b->relacio)
            snprintf(head, sizeof head, "%s · %s", b->nom ? b->nom : "", b->relacio);
        else
            snprintf(head, sizeof head, "%s:%d", b->nom ? b->nom : "", b->linia);
        x = l->doc_x + 1 + cm_put(y, l->doc_x + 1, head, l->doc_w / 2, a ? a : A_BOLD);
        if (!b->relacio && b->context)
            cm_put(y, x + 1, b->context, l->doc_x + l->doc_w - x - 2, a ? a : ft_attr(FT_DIM));
    }
}

static void status(RView *rv)
{
    const char *hint;
    char s[400];

    cm_blank(LINES - 1, 0, COLS, ft_attr(FT_BAR));
    if (rv->msg[0]) {
        cm_put(LINES - 1, 1, rv->msg, COLS - 2, ft_attr(FT_BAR));
        return;
    }
    if (rv->marking) {
        cm_put(LINES - 1, 1, "Marca: ↑↓ paràgrafs · c copia · C copia amb [[referència]] · Esc cancel·la",
               COLS - 2, ft_attr(FT_BAR));
        return;
    }
    switch (rv->focus) {
    case F_NAV:  hint = "↑↓ tria · Retorn fitxes · 1 2 3 carpetes/etiquetes/tipus · Tab · Esc menú · F1"; break;
    case F_LIST: hint = "↑↓ tria · Retorn llegeix · e edita · n nova · / filtra · o ordena · Esc menú · F1"; break;
    case F_DOC:  hint = "Tab enllaç · Retorn segueix · Retrocés torna · m marca per copiar · f cerca · u backlinks · Esc menú"; break;
    default:     hint = "↑↓ tria · Retorn obre la nota · Tab · Esc menú · F1"; break;
    }
    snprintf(s, sizeof s, "%s", hint);
    cm_put(LINES - 1, 1, s, COLS - 2, ft_attr(FT_BAR));
}

static void header(RView *rv)
{
    char s[600], n[32];
    const char *root = vault_root(rv->v), *base = strrchr(root, '/');

    vault_fmt_num(n, sizeof n, (long long)rv->m.n);
    snprintf(s, sizeof s, "Recerca · %s · %s fitxes", base && base[1] ? base + 1 : root, n);
    cm_blank(0, 0, COLS, ft_attr(FT_BAR));
    cm_put(0, 1, s, COLS - 2, ft_attr(FT_BAR));
}

static void paint(RView *rv)
{
    Lay l = layout(rv);

    erase();
    header(rv);
    if (l.nav_w)
        paint_nav(rv, &l);
    if (l.list_w)
        paint_list(rv, &l);
    if (l.nav_w && l.list_w)
        sep_line(l.list_x - 1, l.y0, l.y1);
    if (!rv->maxi)
        sep_line(l.doc_x - 1, l.y0, l.y1);
    paint_doc(rv, &l);
    if (l.back_y0 >= 0)
        paint_back(rv, &l);
    status(rv);
}

/* paint() per als diàlegs de comu/ui */
static void paint_cb(void *ud)
{
    paint(ud);
}

/* ---------- canvis d'estat ---------- */

static void open_doc(RView *rv, long long id)
{
    if (rv->doc.id != id || !rv->doc.buf)
        doc_load(rv, id);
}

/* Refà la llista i hi manté la nota seleccionada si hi és */
static void rebuild_list(RView *rv, int keep)
{
    const MNote *was = keep ? cur_note(rv) : NULL;
    long long id = was ? was->id : 0;
    char *f = cm_fold(rv->filter, strlen(rv->filter));

    free(rv->list);
    rv->nlist = model_list(&rv->m, cur_nav(rv), f, rv->sort, &rv->list);
    free(f);
    rv->list_sel = 0;
    if (id)
        for (size_t i = 0; i < rv->nlist; i++)
            if (rv->m.notes[rv->list[i]].id == id) {
                rv->list_sel = (long)i;
                break;
            }
    if (cur_note(rv) && rv->nhist == 0)
        open_doc(rv, cur_note(rv)->id);
}

static void list_moved(RView *rv)
{
    const MNote *n = cur_note(rv);

    rv->nhist = 0;                  /* el visor torna a seguir la llista */
    if (n)
        open_doc(rv, n->id);
}

static void set_mode(RView *rv, NavMode mode)
{
    model_set_mode(&rv->m, mode);
    rv->nav_sel = rv->nav_top = 0;
    rebuild_list(rv, 1);
    rv->focus = F_NAV;
}

/* Va a una nota (des d'un enllaç o un backlink), guardant on érem */
static void go_note(RView *rv, long long id, long line, const char *section, int block)
{
    Doc *d = &rv->doc;
    Lay l = layout(rv);

    if (d->buf && rv->nhist < (int)(sizeof rv->hist / sizeof *rv->hist)) {
        rv->hist[rv->nhist++] = (Hist){ d->id, d->top, d->link };
    } else if (d->buf) {
        memmove(rv->hist, rv->hist + 1, sizeof rv->hist - sizeof *rv->hist);
        rv->hist[rv->nhist - 1] = (Hist){ d->id, d->top, d->link };
    }
    if (doc_load(rv, id))
        return;
    doc_wrap(d, l.doc_w - 2);
    if (section && *section) {
        long fl = find_section(d, section, block);

        if (fl >= 0)
            d->top = row_of_line(d, (size_t)fl);
        else
            snprintf(rv->msg, sizeof rv->msg, "No hi ha la secció «%s»", section);
    } else if (line > 0) {
        long r = row_of_line(d, (size_t)line - 1);

        d->top = r > 2 ? r - 2 : 0;
    }
    rv->focus = F_DOC;
}

static void go_back(RView *rv)
{
    Hist h;

    if (!rv->nhist) {
        rv->focus = rv->maxi ? F_DOC : F_LIST;
        return;
    }
    h = rv->hist[--rv->nhist];
    if (doc_load(rv, h.id))
        return;
    rv->doc.top = h.top;
    rv->doc.link = h.link;
}

static void follow(RView *rv, const DLink *lk)
{
    long long id;

    if (!lk->target[0]) {                       /* [[#Secció]]: aquesta mateixa nota */
        long fl = lk->section ? find_section(&rv->doc, lk->section, lk->block) : -1;

        if (fl >= 0)
            rv->doc.top = row_of_line(&rv->doc, (size_t)fl);
        return;
    }
    {
        long mi = model_find(&rv->m, rv->doc.id);

        id = vault_resolve(rv->v, mi >= 0 ? rv->m.notes[mi].cami : NULL, lk->target, strlen(lk->target));
    }
    if (id < 0)
        snprintf(rv->msg, sizeof rv->msg, "«%s» és un adjunt: no és al vault de text", lk->target);
    else if (id == 0)
        snprintf(rv->msg, sizeof rv->msg, "Enllaç trencat: no hi ha cap nota «%s»", lk->target);
    else
        go_note(rv, id, 0, lk->section, lk->block);
}

/* Enllaç següent o anterior; el visor s'hi desplaça */
static void next_link(RView *rv, int dir)
{
    Doc *d = &rv->doc;
    Lay l = layout(rv);
    int rows = l.doc_y1 - l.y0;
    long r;

    if (!d->nlinks) {
        snprintf(rv->msg, sizeof rv->msg, "Aquesta fitxa no té enllaços");
        return;
    }
    if (d->link < 0) {                          /* el primer que es veu */
        long k = dir > 0 ? 0 : (long)d->nlinks - 1;

        for (; k >= 0 && k < (long)d->nlinks; k += dir) {
            r = row_of_link(d, k);
            if (dir > 0 ? r >= d->top : r < d->top + rows)
                break;
        }
        d->link = k < 0 || k >= (long)d->nlinks ? (dir > 0 ? 0 : (long)d->nlinks - 1) : k;
    } else {
        d->link = (d->link + dir + (long)d->nlinks) % (long)d->nlinks;
    }
    r = row_of_link(d, d->link);
    if (r < d->top || r >= d->top + rows)
        d->top = r - rows / 3;
}

/* ---------- diàlegs ---------- */

/* Llista modal: Retorn torna l'índex triat, Esc -1 */
static int pick(RView *rv, const char *title, char **lines, size_t n, const char *empty)
{
    long sel = 0, top = 0;

    for (;;) {
        int x0 = COLS > 100 ? (COLS - 96) / 2 : 1, w = COLS > 100 ? 96 : COLS - 2;
        int y0 = 2, rows = LINES - y0 - 4, key;

        if (rows < 3)
            rows = 3;
        if ((long)n < rows)
            rows = n ? (int)n : 1;
        scroll_into(&top, sel, rows);
        paint(rv);
        attron(ft_attr(FT_DIM));
        mvaddstr(y0, x0, "┌");
        mvaddstr(y0 + rows + 1, x0, "└");
        for (int c = 0; c < w - 2; c++) {
            mvaddstr(y0, x0 + 1 + c, "─");
            mvaddstr(y0 + rows + 1, x0 + 1 + c, "─");
        }
        mvaddstr(y0, x0 + w - 1, "┐");
        mvaddstr(y0 + rows + 1, x0 + w - 1, "┘");
        attroff(ft_attr(FT_DIM));
        cm_put(y0, x0 + 2, title, w - 4, ft_attr(FT_HEAD));
        for (int r = 0; r < rows; r++) {
            long i = top + r;
            int y = y0 + 1 + r;
            attr_t a = i == sel && n ? ft_attr(FT_SEL) : 0;

            attron(ft_attr(FT_DIM));
            mvaddstr(y, x0, "│");
            mvaddstr(y, x0 + w - 1, "│");
            attroff(ft_attr(FT_DIM));
            cm_blank(y, x0 + 1, w - 2, a);
            if (!n && r == 0)
                cm_put(y, x0 + 2, empty, w - 4, ft_attr(FT_DIM));
            else if (i < (long)n)
                cm_put(y, x0 + 2, lines[i], w - 4, a);
        }
        cm_blank(LINES - 1, 0, COLS, ft_attr(FT_BAR));
        cm_put(LINES - 1, 1, "↑↓ tria · Retorn obre · Esc torna", COLS - 2, ft_attr(FT_BAR));
        refresh();
        key = key_read();
        if (key < 0 || key == K_FN(KEY_RESIZE))
            continue;
        if (key == 27 || key == 'q')
            return -1;
        if (key == '\n')
            return n ? (int)sel : -1;
        if (key == K_FN(KEY_UP) && sel > 0)
            sel--;
        else if (key == K_FN(KEY_DOWN) && sel + 1 < (long)n)
            sel++;
        else if (key == K_FN(KEY_PPAGE))
            sel = sel > rows ? sel - rows : 0;
        else if (key == K_FN(KEY_NPAGE))
            sel = sel + rows < (long)n ? sel + rows : (long)n - 1;
        else if (key == K_FN(KEY_HOME))
            sel = 0;
        else if (key == K_FN(KEY_END) && n)
            sel = (long)n - 1;
    }
}

static void free_lines(char **l, size_t n)
{
    for (size_t i = 0; i < n; i++)
        free(l[i]);
    free(l);
}

/* l: els enllaços de la fitxa del visor, amb on van a parar */
static void act_outgoing(RView *rv)
{
    Doc *d = &rv->doc;
    char **lines;
    int k;

    if (!d->buf)
        return;
    lines = xmalloc((d->nlinks ? d->nlinks : 1) * sizeof *lines);
    for (size_t i = 0; i < d->nlinks; i++) {
        long me = model_find(&rv->m, d->id);
        long long id = vault_resolve(rv->v, me >= 0 ? rv->m.notes[me].cami : NULL, d->links[i].target,
                                     strlen(d->links[i].target));
        long mi = id > 0 ? model_find(&rv->m, id) : -1;
        char s[700];

        snprintf(s, sizeof s, "%zu: %s%s%s → %s", d->links[i].line, d->links[i].target[0] ? d->links[i].target : "(aquesta)",
                 d->links[i].section ? "#" : "", d->links[i].section ? d->links[i].section : "",
                 !d->links[i].target[0] ? "secció" : id < 0 ? "adjunt" : mi >= 0 ? rv->m.notes[mi].cami : "TRENCAT");
        lines[i] = xstrdup(s);
    }
    k = pick(rv, "Enllaços d'aquesta fitxa", lines, d->nlinks, "Aquesta fitxa no té enllaços.");
    free_lines(lines, d->nlinks);
    if (k >= 0) {
        d->link = k;
        follow(rv, &d->links[k]);
    }
}

/* t: enllaços trencats de tot el vault */
static void act_broken(RView *rv)
{
    sqlite3_stmt *s;
    char **lines = NULL;
    long long *ids = NULL;
    int *ln = NULL;
    size_t n = 0;
    int k;

    if (sqlite3_prepare_v2(vault_db(rv->v), "SELECT f.id, f.cami, e.linia, e.desti FROM enllacos e "
                                            "JOIN fitxes f ON f.id = e.de WHERE e.a IS NULL AND NOT e.adjunt "
                                            "ORDER BY f.cami, e.linia", -1, &s, NULL) != SQLITE_OK)
        return;
    while (sqlite3_step(s) == SQLITE_ROW) {
        char x[900];

        lines = xrealloc(lines, (n + 1) * sizeof *lines);
        ids = xrealloc(ids, (n + 1) * sizeof *ids);
        ln = xrealloc(ln, (n + 1) * sizeof *ln);
        ids[n] = sqlite3_column_int64(s, 0);
        ln[n] = sqlite3_column_int(s, 2);
        snprintf(x, sizeof x, "%s:%d  [[%s]]", sqlite3_column_text(s, 1), ln[n], sqlite3_column_text(s, 3));
        lines[n++] = xstrdup(x);
    }
    sqlite3_finalize(s);
    k = pick(rv, "Enllaços trencats", lines, n, "No hi ha cap enllaç trencat.");
    if (k >= 0)
        go_note(rv, ids[k], ln[k], NULL, 0);
    free_lines(lines, n);
    free(ids);
    free(ln);
}

/* ---------- cercar al contingut ---------- */

/* El fitxer sencer (cal alliberar-lo), o NULL */
static char *slurp_file(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    char *buf = NULL;
    size_t cap = 0, n = 0, r;

    if (!f)
        return NULL;
    do {
        if (cap - n < 4096)
            buf = xrealloc(buf, cap = cap ? 2 * cap : 16384);
        r = fread(buf + n, 1, cap - n - 1, f);
        n += r;
    } while (r > 0);
    fclose(f);
    buf[n] = '\0';
    *len = n;
    return buf;
}

/* Les paraules de q (lletres i xifres; la resta separa), plegades amb cm_fold */
static size_t words_of(const char *q, char ***out)
{
    char **v = NULL;
    size_t n = 0;

    for (const char *p = q; *p;) {
        size_t l = 0;

        while (p[l] && ((unsigned char)p[l] >= 0x80 || isalnum((unsigned char)p[l])))
            l++;
        if (l) {
            v = xrealloc(v, (n + 1) * sizeof *v);
            v[n++] = cm_fold(p, l);
            p += l;
        } else {
            p++;
        }
    }
    *out = v;
    return n;
}

/* La primera línia de text[0..len) (des de la línia from, 0 = la primera) que
 * conté totes les paraules; si phrase, la frase sencera. Retorna la línia (1
 * = la primera) o 0, i en copia el text a ctx. */
static int find_line(const char *text, size_t len, size_t from, char **w, size_t nw,
                     const char *phrase, char *ctx, size_t ctxsz)
{
    size_t line = 0;

    for (size_t i = 0; i < len; line++) {
        size_t e = i;
        int ok = 1;
        char *f;

        while (e < len && text[e] != '\n')
            e++;
        if (line >= from && e > i) {
            f = cm_fold(text + i, e - i);
            if (phrase)
                ok = strstr(f, phrase) != NULL;
            else
                for (size_t k = 0; k < nw && ok; k++)
                    ok = strstr(f, w[k]) != NULL;
            free(f);
            if (ok) {
                const char *s = text + i;

                while (s < text + e && (*s == ' ' || *s == '\t' || *s == '#' || *s == '>' || *s == '-'))
                    s++;
                snprintf(ctx, ctxsz, "%.*s", (int)(text + e - s), s);
                return (int)line + 1;
            }
        }
        i = e + 1;
    }
    return 0;
}

/* «f»: cerca les paraules (o «una frase» entre cometes) al nom, a les
 * propietats i al text de totes les fitxes, amb l'índex FTS5; la llista
 * mostra la primera línia on surten, i Retorn hi va */
static void act_search(RView *rv)
{
    static char q[200];
    char **w = NULL, *expr, *phrase = NULL, title[300];
    size_t nw, cap, el = 0, n = 0;
    sqlite3_stmt *s;
    char **lines = NULL;
    long long *ids = NULL;
    int *ln = NULL, k;

    if (!cm_prompt("Cerca a les fitxes (\"frase exacta\"): ", q, q, sizeof q, paint_cb, rv))
        return;
    nw = words_of(q, &w);
    if (!nw) {
        free(w);
        return;
    }
    /* L'expressió de FTS5: cada paraula entre cometes (així cap caràcter de
     * l'usuari no és un operador), totes alhora. L'índex no guarda les
     * posicions (detail=none): una frase («entre cometes») es busca com les
     * seves paraules, i després es comprova al text de cada fitxa. */
    cap = strlen(q) * 2 + nw * 8 + 8;
    expr = xmalloc(cap);
    for (size_t i = 0; i < nw; i++)
        el += (size_t)snprintf(expr + el, cap - el, "%s\"%s\"", i ? " AND " : "", w[i]);
    {
        const char *t = q;

        while (*t == ' ')
            t++;
        if (*t == '"' && nw > 1) {
            size_t pl = 0;

            phrase = xmalloc(strlen(q) + nw + 1);
            for (size_t i = 0; i < nw; i++)
                pl += (size_t)sprintf(phrase + pl, "%s%s", i ? " " : "", w[i]);
        }
    }
    if (sqlite3_prepare_v2(vault_db(rv->v), "SELECT f.id, f.cami FROM text JOIN fitxes f ON f.id = text.rowid "
                                            "WHERE text MATCH ? ORDER BY f.cami LIMIT 500", -1, &s, NULL) != SQLITE_OK) {
        snprintf(rv->msg, sizeof rv->msg, "No es pot cercar: %s", sqlite3_errmsg(vault_db(rv->v)));
        goto out;
    }
    sqlite3_bind_text(s, 1, expr, -1, SQLITE_STATIC);
    while (sqlite3_step(s) == SQLITE_ROW) {
        const char *cami = (const char *)sqlite3_column_text(s, 1);
        char path[4600], ctx[300] = "", x[900], *buf;
        size_t len, body = 0;
        int line = 0;

        snprintf(path, sizeof path, "%s/%s", vault_root(rv->v), cami);
        if ((buf = slurp_file(path, &len))) {
            size_t body_line = 1;

            body = cm_front(buf, len, &body_line, NULL, NULL) ? body_line - 1 : 0;
            line = find_line(buf, len, body, w, nw, phrase, ctx, sizeof ctx);
            if (!line && phrase) {          /* la frase: a la capçalera, o enlloc */
                char *f = cm_fold(buf, len);
                int in = strstr(f, phrase) != NULL;

                free(f);
                free(buf);
                if (!in)
                    continue;
                buf = NULL;
            }
            free(buf);
        }
        if (line)
            snprintf(x, sizeof x, "%s:%d  %s", cami, line, ctx);
        else
            snprintf(x, sizeof x, "%s  (al nom o a les propietats)", cami);
        lines = xrealloc(lines, (n + 1) * sizeof *lines);
        ids = xrealloc(ids, (n + 1) * sizeof *ids);
        ln = xrealloc(ln, (n + 1) * sizeof *ln);
        ids[n] = sqlite3_column_int64(s, 0);
        ln[n] = line;
        lines[n++] = xstrdup(x);
    }
    if (sqlite3_errcode(vault_db(rv->v)) != SQLITE_DONE && sqlite3_errcode(vault_db(rv->v)) != SQLITE_ROW)
        snprintf(rv->msg, sizeof rv->msg, "Error en cercar: %s", sqlite3_errmsg(vault_db(rv->v)));
    sqlite3_finalize(s);
    snprintf(title, sizeof title, "Cerca «%s»: %zu %s%s", q, n, n == 1 ? "fitxa" : "fitxes",
             n == 500 ? " (les 500 primeres)" : "");
    k = pick(rv, title, lines, n, "Cap fitxa no conté aquestes paraules.");
    if (k >= 0) {
        go_note(rv, ids[k], ln[k], NULL, 0);
        rv->focus = F_DOC;
        rv->hint_id = ln[k] > 0 ? ids[k] : 0;
        rv->hint_line = ln[k] > 0 ? (size_t)ln[k] - 1 : 0;
    }
    free_lines(lines, n);
    free(ids);
    free(ln);
out:
    for (size_t i = 0; i < nw; i++)
        free(w[i]);
    free(w);
    free(expr);
    free(phrase);
}

static int menu_run(RView *rv)
{
    static const CmMenuItem items[] = {
        { "Navega", NULL, 0 },
        { "Per carpetes", "1", '1' },
        { "Per etiquetes", "2", '2' },
        { "Per tipus", "3", '3' },
        { "Filtra les fitxes pel nom", "/", '/' },
        { "Cerca al contingut de totes", "f", 'f' },
        { "Canvia l'ordre", "o", 'o' },
        { "Fitxa", NULL, 0 },
        { "Edita a Phileas", "e", 'e' },
        { "Marca paràgrafs per copiar", "m", 'm' },
        { "Fitxa nova", "n", 'n' },
        { "Anotació a aquesta fitxa", "a", 'a' },
        { "Mou a la paperera", "Supr", K_FN(KEY_DC) },
        { "Enllaços", NULL, 0 },
        { "Enllaç següent / anterior", "Tab Maj+Tab", ']' },
        { "Segueix l'enllaç", "Retorn", '\n' },
        { "Torna enrere", "Retrocés", K_FN(KEY_BACKSPACE) },
        { "Hi enllacen (backlinks)", "u", 'u' },
        { "Mostra o amaga els backlinks", "b", 'b' },
        { "Enllaços d'aquesta fitxa", "l", 'l' },
        { "Visor a tota la pantalla", "z", 'z' },
        { "Recerca", NULL, 0 },
        { "Enllaços trencats", "t", 't' },
        { "Torna a llegir el vault", "Ctrl+R", 18 },
        { "Rep del PC (rep)", "r", 'r' },
        { "Ajuda", "F1", K_FN(KEY_F(1)) },
        { "Surt", "Ctrl+Q", 17 },
    };

    return cm_menu(items, (int)(sizeof items / sizeof *items), paint_cb, rv);
}

static void help(void)
{
    static const char *const lines[] = {
        "Recerca · ajuda",
        "",
        "  Tab / Maj+Tab              panell següent / anterior; al visor, enllaç següent / anterior",
        "  1 / 2 / 3                  navega per carpetes / etiquetes (#a/b en arbre) / tipus",
        "  ↑ ↓ RePàg AvPàg Inici Fi   moure's; a la navegació, la llista mostra la branca triada",
        "  Retorn / →                 navegació → fitxes → visor; al visor, segueix l'enllaç triat",
        "  ←                          torna al panell anterior (del visor, a les fitxes)",
        "  /                          filtra les fitxes pel nom (sense majúscules ni accents)",
        "  f                          cerca al nom, les propietats i el text de totes les fitxes",
        "                             (\"entre cometes\": la frase exacta); Retorn va a la línia",
        "  o                          ordena per nom, data, modificació o paraules",
        "  Tab / Maj+Tab (o ] / [)    al visor: enllaç següent / anterior (Retorn tria el primer)",
        "  Retrocés                   torna a la fitxa d'abans (després de seguir un enllaç)",
        "  u                          notes que enllacen a aquesta (backlinks) · b els mostra o amaga",
        "  l                          enllaços d'aquesta fitxa i on van a parar",
        "  t                          enllaços trencats de tot el vault",
        "  z                          visor a tota la pantalla (i tornar)",
        "  e                          edita la fitxa del visor a Phileas (a la línia que es veu)",
        "  m                          marca paràgrafs del visor: ↑↓ n'amplia la marca, c copia,",
        "                             C copia amb [[la fitxa]] al final; a Phileas, Ctrl+V",
        "  n                          fitxa nova a la carpeta triada (amb plantilla, si n'hi ha)",
        "  a                          anotació: una fitxa nova que enllaça a la del visor",
        "  Supr                       mou la fitxa a la paperera d'Obsidian (.trash/)",
        "  Ctrl+R                     torna a llegir el vault (només el que ha canviat)",
        "  r                          a la Pi: rep el vault del PC i hi envia _pi/ (rep), i el torna a llegir",
        "  Esc                        esborra el filtre; sense filtre, el menú",
        "  Ctrl+P                     el menú (com la paleta de Phileas)",
        "  Ctrl+L                     redibuixa",
        "  Ctrl+Q / q                 surt",
        "",
        "  L'índex és a .recerca/ dins del vault; es pot esborrar sense perdre res.",
        "  En una rèplica del PC (recerca-replica = si) només s'escriu a la carpeta d'entrada (_pi/).",
        "",
        "  Premeu una tecla per tornar.",
    };

    cm_help(lines, sizeof lines / sizeof *lines);
}

static void reload(RView *rv)
{
    VaultSync s;
    char err[200];
    long long doc = rv->doc.id;
    const MNav *nv = cur_nav(rv);
    char *navkey = nv && nv->key ? xstrdup(nv->key) : NULL;

    if (vault_sync(rv->v, 0, NULL, NULL, &s, err, sizeof err)) {
        snprintf(rv->msg, sizeof rv->msg, "%s", err);
        free(navkey);
        return;
    }
    model_load(&rv->m, vault_db(rv->v));
    rv->nav_sel = 0;
    if (navkey)
        for (size_t i = 0; i < rv->m.nnav; i++)
            if (rv->m.nav[i].key && !strcmp(rv->m.nav[i].key, navkey)) {
                rv->nav_sel = (long)i;
                break;
            }
    free(navkey);
    rebuild_list(rv, 1);
    back_free(rv);
    if (doc && model_find(&rv->m, doc) >= 0) {
        long top = rv->doc.top;

        doc_load(rv, doc);
        rv->doc.top = top;
    } else if (cur_note(rv)) {          /* la fitxa del visor ja no hi és */
        rv->nhist = 0;
        doc_load(rv, cur_note(rv)->id);
    } else {
        doc_free(&rv->doc);
    }
    snprintf(rv->msg, sizeof rv->msg, "Vault llegit: %zu noves, %zu canviades, %zu esborrades",
             s.noves, s.canviades, s.esborrades);
}

/* ---------- escriure ---------- */

/* A la rèplica només es pot escriure dins de la carpeta d'entrada */
static int writable(const RView *rv, const char *rel)
{
    size_t n = strlen(rv->opt.entrada);

    return !rv->opt.replica || (!strncmp(rel, rv->opt.entrada, n) && rel[n] == '/');
}

/* Torna a llegir una nota (o la treu) i ho posa tot al dia; selecciona
 * select_id a la llista si hi és i el mostra al visor. */
static void after_write(RView *rv, const char *rel, long long select_id)
{
    char err[200];
    long top = rv->doc.top;
    long long doc = select_id ? select_id : rv->doc.id;

    if (vault_reindex(rv->v, rel, err, sizeof err)) {
        snprintf(rv->msg, sizeof rv->msg, "%s", err);
        return;
    }
    model_load(&rv->m, vault_db(rv->v));
    if (rv->nav_sel >= (long)rv->m.nnav)
        rv->nav_sel = 0;
    back_free(rv);
    rebuild_list(rv, !select_id);
    if (select_id)
        for (size_t i = 0; i < rv->nlist; i++)
            if (rv->m.notes[rv->list[i]].id == select_id)
                rv->list_sel = (long)i;
    rv->doc.id = 0;
    if (doc && model_find(&rv->m, doc) >= 0) {
        doc_load(rv, doc);
        if (!select_id)
            rv->doc.top = top;
    } else if (cur_note(rv)) {
        doc_load(rv, cur_note(rv)->id);
    }
}

static long long id_of(const RView *rv, const char *rel)
{
    for (size_t i = 0; i < rv->m.n; i++)
        if (!strcmp(rv->m.notes[i].cami, rel))
            return rv->m.notes[i].id;
    return 0;
}

static void edit_rel(RView *rv, const char *rel, size_t line)
{
    char path[8192], err[300];
    char *r = xstrdup(rel);

    snprintf(path, sizeof path, "%s/%s", vault_root(rv->v), rel);
    if (launch_editor(path, line, err, sizeof err))
        snprintf(rv->msg, sizeof rv->msg, "%s", err);
    after_write(rv, r, 0);
    free(r);
}

/* La nota del visor, o NULL */
static const MNote *doc_note(const RView *rv)
{
    long i = rv->doc.buf ? model_find(&rv->m, rv->doc.id) : -1;

    return i >= 0 ? &rv->m.notes[i] : NULL;
}

/* e: obre la fitxa del visor a Phileas, a la línia que es veu (o la de
 * l'enllaç triat) */
static void act_edit(RView *rv)
{
    const MNote *n = doc_note(rv);
    Doc *d = &rv->doc;
    size_t line = 1;

    if (!n)
        return;
    if (!writable(rv, n->cami)) {
        snprintf(rv->msg, sizeof rv->msg, "És una còpia del PC: no s'edita aquí. «a» hi fa una anotació a %s/",
                 rv->opt.entrada);
        return;
    }
    if (d->link >= 0 && d->link < (long)d->nlinks)
        line = d->links[d->link].line;
    else
        for (long r = d->top; r < (long)d->nrows; r++)
            if (d->rows[r].kind == R_BODY || d->rows[r].kind == R_BLANK) {
                line = d->rows[r].idx + 1;
                break;
            }
    edit_rel(rv, n->cami, line);
}

/* Llegeix una plantilla del vault (o NULL si no se'n tria cap) */
static char *choose_template(RView *rv)
{
    char **l, **lines, *text = NULL;
    size_t n = nova_plantilles(vault_root(rv->v), &l);
    int k;

    if (!n)
        return NULL;
    lines = xmalloc((n + 1) * sizeof *lines);
    lines[0] = xstrdup("(sense plantilla)");
    for (size_t i = 0; i < n; i++)
        lines[i + 1] = xstrdup(l[i]);
    k = pick(rv, "Plantilla de la fitxa nova", lines, n + 1, "");
    if (k > 0) {
        char path[8192];
        FILE *f;

        snprintf(path, sizeof path, "%s/%s", vault_root(rv->v), l[k - 1]);
        if ((f = fopen(path, "rb"))) {
            size_t cap = 4096, m = 0, r;

            text = xmalloc(cap + 1);
            while ((r = fread(text + m, 1, cap - m, f)) > 0)
                if ((m += r) == cap)
                    text = xrealloc(text, (cap *= 2) + 1);
            text[m] = '\0';
            fclose(f);
        }
    }
    free_lines(lines, n + 1);
    nova_llista_free(l, n);
    return k < 0 ? (char *)-1 : text;
}

static void now_str(char *out, size_t sz)
{
    time_t t = time(NULL);
    struct tm tm;

    localtime_r(&t, &tm);
    strftime(out, sz, "%Y-%m-%d %H:%M", &tm);
}

/* Crea la fitxa i l'obre a Phileas */
static void create_and_edit(RView *rv, const char *carpeta, const char *titol, const char *plantilla,
                            const char *anota)
{
    char *nom = nova_nom(titol), *text, id[64], ara[32], rel[4096], err[300];
    long long nid;

    nova_id(rv->opt.replica ? "pi-" : "", id, sizeof id);
    now_str(ara, sizeof ara);
    text = nova_text(plantilla, titol, id, ara, anota);
    if (nova_crea(vault_root(rv->v), carpeta, nom, text, rel, sizeof rel, err, sizeof err)) {
        snprintf(rv->msg, sizeof rv->msg, "%s", err);
    } else {
        after_write(rv, rel, 0);            /* a l'índex i a la llista */
        edit_rel(rv, rel, 1);               /* a Phileas; en tornar, es torna a llegir */
        nid = id_of(rv, rel);
        if (nid) {
            for (size_t i = 0; i < rv->nlist; i++)
                if (rv->m.notes[rv->list[i]].id == nid)
                    rv->list_sel = (long)i;
            rv->nhist = 0;
            doc_load(rv, nid);
        }
        snprintf(rv->msg, sizeof rv->msg, "Fitxa nova: %s", rel);
    }
    free(text);
    free(nom);
}

/* n: fitxa nova a la carpeta triada (a la rèplica, a la d'entrada) */
static void act_new(RView *rv)
{
    char titol[300], carpeta[4096] = "";
    char *plantilla;
    const MNav *nv = cur_nav(rv);

    if (rv->opt.replica)
        snprintf(carpeta, sizeof carpeta, "%s", rv->opt.entrada);
    else if (rv->m.mode == NAV_CARPETES && nv && nv->key)   /* la carpeta triada; si no, l'arrel */
        snprintf(carpeta, sizeof carpeta, "%s", nv->key);
    {
        char label[600];

        snprintf(label, sizeof label, "Fitxa nova a %s/: ", *carpeta ? carpeta : "(arrel)");
        if (!cm_prompt(label, NULL, titol, sizeof titol, paint_cb, rv) || !titol[0])
            return;
    }
    plantilla = choose_template(rv);
    if (plantilla == (char *)-1)
        return;
    create_and_edit(rv, carpeta, titol, plantilla, NULL);
    free(plantilla);
}

/* a: anotació a la fitxa del visor (una fitxa nova que hi enllaça) */
static void act_annotate(RView *rv)
{
    const MNote *n = doc_note(rv);
    char titol[300], ini[300], ara[32], carpeta[4096], nom[256];

    if (!n)
        return;
    snprintf(nom, sizeof nom, "%s", n->nom);
    snprintf(carpeta, sizeof carpeta, "%s", rv->opt.replica ? rv->opt.entrada : n->carpeta);
    now_str(ara, sizeof ara);
    snprintf(ini, sizeof ini, "Anotació a %s %.10s", nom, ara);
    if (!cm_prompt("Anotació: ", ini, titol, sizeof titol, paint_cb, rv) || !titol[0])
        return;
    create_and_edit(rv, carpeta, titol, NULL, nom);
}

/* Supr: la fitxa triada (llista) o la del visor, a .trash/ */
static void act_trash(RView *rv)
{
    const MNote *n = rv->focus == F_LIST ? cur_note(rv) : doc_note(rv);
    char q[600], dest[4096], err[300], *rel;

    if (!n)
        return;
    if (!writable(rv, n->cami)) {
        snprintf(rv->msg, sizeof rv->msg, "És una còpia del PC: aquí només es poden esborrar les fitxes de %s/",
                 rv->opt.entrada);
        return;
    }
    snprintf(q, sizeof q, "Moure «%s» a la paperera (.trash/)?", n->nom);
    if (!cm_confirm(q, paint_cb, rv))
        return;
    rel = xstrdup(n->cami);
    if (nova_paperera(vault_root(rv->v), rel, dest, sizeof dest, err, sizeof err)) {
        snprintf(rv->msg, sizeof rv->msg, "%s", err);
    } else {
        rv->nhist = 0;
        after_write(rv, rel, 0);
        snprintf(rv->msg, sizeof rv->msg, "A la paperera: %s", dest);
    }
    free(rel);
}

/* r: a la rèplica (la Pi), executa «rep VAULT» i torna a llegir el vault */
static void act_rep(RView *rv)
{
    static const char *script =
        "R=$(command -v rep || echo /data/recerca/rep); sh \"$R\" \"$1\"; "
        "printf '\\nPrem Retorn per tornar a Recerca.'; read x";
    char *argv[] = { "sh", "-c", (char *)script, "rep", (char *)vault_root(rv->v), NULL };
    char err[200];

    if (!rv->opt.replica) {
        snprintf(rv->msg, sizeof rv->msg, "«rep» només hi és en les còpies del PC (a la Pi)");
        return;
    }
    if (launch_program(argv, err, sizeof err))
        snprintf(rv->msg, sizeof rv->msg, "rep: %s", err);
    rv->nhist = 0;
    reload(rv);
}

/* ---------- tecles ---------- */

static void filter_key(RView *rv, int key)
{
    size_t l = strlen(rv->filter);

    if (key == '\n' || key == 9) {
        rv->editing = 0;
    } else if (key == 27) {
        rv->editing = 0;
        rv->filter[0] = '\0';
        rebuild_list(rv, 1);
    } else if (key == K_FN(KEY_BACKSPACE) || key == 127 || key == 8) {
        while (l && ((unsigned char)rv->filter[l - 1] & 0xC0) == 0x80)
            l--;
        if (l)
            rv->filter[--l] = '\0';
        rebuild_list(rv, 1);
    } else if (key == 21) {
        rv->filter[0] = '\0';
        rebuild_list(rv, 1);
    } else if (key >= 32 && !(key & (KEY_FLAG_FN | KEY_FLAG_ALT))) {
        char enc[4];
        size_t k = utf8_encode((wchar_t)key, enc);

        if (l + k < sizeof rv->filter) {
            memcpy(rv->filter + l, enc, k);
            rv->filter[l + k] = '\0';
        }
        rebuild_list(rv, 1);
    }
}

static void move_sel(long *sel, long n, int key, int page)
{
    if (n <= 0)
        return;
    switch (key) {
    case K_FN(KEY_UP):    if (*sel > 0) (*sel)--; break;
    case K_FN(KEY_DOWN):  if (*sel + 1 < n) (*sel)++; break;
    case K_FN(KEY_PPAGE): *sel = *sel > page ? *sel - page : 0; break;
    case K_FN(KEY_NPAGE): *sel = *sel + page < n ? *sel + page : n - 1; break;
    case K_FN(KEY_HOME):  *sel = 0; break;
    case K_FN(KEY_END):   *sel = n - 1; break;
    default: break;
    }
}

static int is_move(int key)
{
    return key == K_FN(KEY_UP) || key == K_FN(KEY_DOWN) || key == K_FN(KEY_PPAGE) ||
           key == K_FN(KEY_NPAGE) || key == K_FN(KEY_HOME) || key == K_FN(KEY_END);
}

static void focus_next(RView *rv, int dir)
{
    int order[4], n = 0, at = 0;

    if (!rv->maxi) {
        order[n++] = F_NAV;
        order[n++] = F_LIST;
    }
    order[n++] = F_DOC;
    if (rv->show_back)
        order[n++] = F_BACK;
    for (int i = 0; i < n; i++)
        if (order[i] == rv->focus)
            at = i;
    rv->focus = order[(at + dir + n) % n];
}

/* ---------- marcar i copiar ---------- */

/* 1 si la línia l del cos té alguna cosa més que espais */
static int body_text(const Doc *d, size_t l)
{
    if (l < d->body || l >= d->nlines)
        return 0;
    for (size_t i = 0; i < d->ll[l]; i++)
        if (d->buf[d->lo[l] + i] != ' ' && d->buf[d->lo[l] + i] != '\t')
            return 1;
    return 0;
}

/* La primera línia del cos amb text (o d->nlines si no n'hi ha) */
static size_t first_text(const Doc *d)
{
    size_t k = d->body;

    while (k < d->nlines && !body_text(d, k))
        k++;
    return k;
}

/* La línia amb text següent (dir = 1) o anterior (-1) a l, o l si no n'hi ha */
static size_t next_text(const Doc *d, size_t l, int dir)
{
    for (size_t k = l; dir > 0 ? k + 1 < d->nlines : k > d->body;) {
        k = dir > 0 ? k + 1 : k - 1;
        if (body_text(d, k))
            return k;
    }
    return l;
}

/* «m»: comença a marcar pel primer paràgraf que es veu al visor */
static void mark_start(RView *rv)
{
    Doc *d = &rv->doc;
    size_t first = (size_t)-1;

    if (!d->buf)
        return;
    if (rv->hint_id == d->id && body_text(d, rv->hint_line)) {   /* la línia de la cerca */
        Lay l = layout(rv);
        long r = row_of_line(d, rv->hint_line);

        if (r >= d->top && r < d->top + (l.doc_y1 - l.y0))
            first = rv->hint_line;
    }
    for (size_t k = d->top < 0 ? 0 : (size_t)d->top; k < d->nrows && first == (size_t)-1; k++)
        if (d->rows[k].kind == R_BODY && body_text(d, d->rows[k].idx))
            first = d->rows[k].idx;
    if (first == (size_t)-1) {
        first = first_text(d);
        if (first >= d->nlines) {
            snprintf(rv->msg, sizeof rv->msg, "Aquesta fitxa no té text per copiar");
            return;
        }
    }
    rv->marking = 1;
    rv->mark_a = rv->mark_b = first;
    rv->focus = F_DOC;
}

/* Que la darrera línia marcada es vegi */
static void mark_visible(RView *rv)
{
    Doc *d = &rv->doc;
    Lay l = layout(rv);
    long rows = l.doc_y1 - l.y0, r = row_of_line(d, rv->mark_b);

    if (r < d->top)
        d->top = r;
    else if (r >= d->top + rows)
        d->top = r - rows + 1;
}

/* Copia les línies marcades (amb ref, i «[[Nom]]» al final) al porta-retalls
 * comú i al del sistema, si n'hi ha */
static void mark_copy(RView *rv, int ref)
{
    Doc *d = &rv->doc;
    size_t lo = rv->mark_a < rv->mark_b ? rv->mark_a : rv->mark_b;
    size_t hi = rv->mark_a < rv->mark_b ? rv->mark_b : rv->mark_a;
    size_t len = d->lo[hi] + d->ll[hi] - d->lo[lo], n = len, par = 0;
    long i = model_find(&rv->m, d->id);
    const char *nom = i >= 0 ? rv->m.notes[i].nom : "";
    char *s = xmalloc(len + strlen(nom) + 16);

    memcpy(s, d->buf + d->lo[lo], len);
    for (size_t k = lo; k <= hi; k++)
        par += body_text(d, k) && (k == lo || !body_text(d, k - 1));
    if (ref && nom[0])
        n += (size_t)sprintf(s + n, "\n\n[[%s]]", nom);
    rv->marking = 0;
    if (cm_retalls_write(s, n, NULL) != 0) {
        snprintf(rv->msg, sizeof rv->msg, "No s'ha pogut copiar: %s", strerror(errno));
    } else {
        if (sysclip_tool())
            sysclip_set(s, n);
        snprintf(rv->msg, sizeof rv->msg, "Copiat: %zu %s%s · a Phileas, Ctrl+V", par,
                 par == 1 ? "paràgraf" : "paràgrafs", ref ? " amb la referència" : "");
    }
    free(s);
}

/* Les tecles mentre es marca */
static void mark_key(RView *rv, int key)
{
    Doc *d = &rv->doc;

    switch (key) {
    case K_FN(KEY_DOWN): case 'j':
        rv->mark_b = next_text(d, rv->mark_b, 1);
        break;
    case K_FN(KEY_UP): case 'k':
        rv->mark_b = next_text(d, rv->mark_b, -1);
        break;
    case K_FN(KEY_HOME):
        if (first_text(d) < d->nlines)
            rv->mark_b = first_text(d);
        break;
    case K_FN(KEY_END):
        for (size_t k = d->nlines; k-- > d->body;)
            if (body_text(d, k)) {
                rv->mark_b = k;
                break;
            }
        break;
    case 'c': case '\n':
        mark_copy(rv, 0);
        return;
    case 'C':
        mark_copy(rv, 1);
        return;
    case 27: case 'm':
        rv->marking = 0;
        snprintf(rv->msg, sizeof rv->msg, "Marca cancel·lada");
        return;
    default:
        return;
    }
    mark_visible(rv);
}

static int pane_key(RView *rv, int key)
{
    Lay l = layout(rv);
    int page = l.y1 - l.y0 - 1;

    switch (rv->focus) {
    case F_NAV:
        if (is_move(key)) {
            long was = rv->nav_sel;

            move_sel(&rv->nav_sel, (long)rv->m.nnav, key, page);
            if (was != rv->nav_sel) {
                rv->filter[0] = '\0';
                rebuild_list(rv, 0);
                list_moved(rv);
            }
            return 1;
        }
        if (key == '\n' || key == K_FN(KEY_RIGHT)) {
            rv->focus = F_LIST;
            return 1;
        }
        return 0;
    case F_LIST:
        if (is_move(key)) {
            long was = rv->list_sel;

            move_sel(&rv->list_sel, (long)rv->nlist, key, page);
            if (was != rv->list_sel)
                list_moved(rv);
            return 1;
        }
        if (key == '\n' || key == K_FN(KEY_RIGHT)) {
            if (cur_note(rv)) {
                list_moved(rv);
                rv->focus = F_DOC;
            }
            return 1;
        }
        if (key == K_FN(KEY_LEFT)) {
            rv->focus = F_NAV;
            return 1;
        }
        return 0;
    case F_DOC: {
        Doc *d = &rv->doc;
        int rows = l.doc_y1 - l.y0;

        switch (key) {
        case K_FN(KEY_UP):    d->top--; return 1;
        case K_FN(KEY_DOWN):  d->top++; return 1;
        case K_FN(KEY_PPAGE): d->top -= rows - 1; return 1;
        case K_FN(KEY_NPAGE): case ' ': d->top += rows - 1; return 1;
        case K_FN(KEY_HOME):  d->top = 0; return 1;
        case K_FN(KEY_END):   d->top = (long)d->nrows; return 1;
        case 9: case ']': next_link(rv, 1); return 1;              /* Tab */
        case K_FN(KEY_BTAB): case '[': next_link(rv, -1); return 1;
        case '\n':
            if (d->link >= 0 && d->link < (long)d->nlinks) {
                follow(rv, &d->links[d->link]);
            } else {
                next_link(rv, 1);               /* el primer que es veu */
                if (d->link >= 0)
                    snprintf(rv->msg, sizeof rv->msg, "Retorn segueix l'enllaç · Tab el següent · Maj+Tab l'anterior");
            }
            return 1;
        case K_FN(KEY_BACKSPACE): case 127: case 8:
            go_back(rv);
            return 1;
        case K_FN(KEY_LEFT):
            if (rv->maxi)
                go_back(rv);
            else
                rv->focus = F_LIST;
            return 1;
        default:
            return 0;
        }
    }
    case F_BACK:
        if (is_move(key)) {
            move_sel(&rv->back_sel, (long)rv->nback, key, 3);
            return 1;
        }
        if (key == '\n' && rv->back_sel < (long)rv->nback) {
            Back b = rv->back[rv->back_sel];

            go_note(rv, b.de, b.relacio ? 0 : b.linia, NULL, 0);
            return 1;
        }
        if (key == K_FN(KEY_LEFT)) {
            rv->focus = F_DOC;
            return 1;
        }
        return 0;
    default:
        return 0;
    }
}

/* RECERCA_TRACE=FITXER: temps de cada tecla (tecla, ms) per mesurar a la Pi */
static FILE  *trace_f;
static double t_key;

static double now_ms(void)
{
    struct timespec t;

    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1e3 + t.tv_nsec / 1e6;
}

void rview_run(Vault *v, const RviewOpts *o)
{
    RView rv;
    int pending = 0, last = 0;
    double t0 = now_ms();

    if (getenv("RECERCA_TRACE"))
        trace_f = fopen(getenv("RECERCA_TRACE"), "a");

    memset(&rv, 0, sizeof rv);
    rv.v = v;
    rv.opt = *o;
    if (!rv.opt.entrada || !*rv.opt.entrada)
        rv.opt.entrada = "_pi";
    rv.doc.link = -1;
    rv.focus = F_NAV;
    if (model_load(&rv.m, vault_db(v)))
        snprintf(rv.msg, sizeof rv.msg, "No es pot llegir l'índex");
    rebuild_list(&rv, 0);
    list_moved(&rv);
    for (;;) {
        int key;

        ft_sync_size();
        paint(&rv);
        refresh();
        if (trace_f) {
            if (last)
                fprintf(trace_f, "tecla %d: %.1f ms\n", last, now_ms() - t_key);
            else
                fprintf(trace_f, "inici: %.1f ms\n", now_ms() - t0);
            fflush(trace_f);
        }
        if (pending) {
            key = pending;
            pending = 0;
        } else {
            key = key_read();
        }
        if (key < 0 || key == K_FN(KEY_RESIZE))
            continue;
        last = key;
        t_key = now_ms();
        rv.msg[0] = '\0';
        if (rv.editing) {
            filter_key(&rv, key);
            continue;
        }
        if (rv.marking) {
            mark_key(&rv, key);
            continue;
        }
        if (pane_key(&rv, key))
            continue;
        switch (key) {
        case 17: case 'q':
            goto out;
        case 9:
            focus_next(&rv, 1);
            break;
        case K_FN(KEY_BTAB):
            focus_next(&rv, -1);
            break;
        case 12:
            clearok(curscr, TRUE);
            break;
        case 18:
            reload(&rv);
            break;
        case 'm':
            mark_start(&rv);
            break;
        case 'f':
            act_search(&rv);
            break;
        case '1': set_mode(&rv, NAV_CARPETES); break;
        case '2': set_mode(&rv, NAV_ETIQUETES); break;
        case '3': set_mode(&rv, NAV_TIPUS); break;
        case '/':
            rv.maxi = 0;
            rv.focus = F_LIST;
            rv.editing = 1;
            break;
        case 'o':
            rv.sort = (rv.sort + 1) % SORT_N;
            rebuild_list(&rv, 1);
            snprintf(rv.msg, sizeof rv.msg, "Fitxes ordenades %s", model_sort_label(rv.sort));
            break;
        case 'z':
            rv.maxi = !rv.maxi;
            if (rv.maxi && rv.focus != F_BACK)
                rv.focus = F_DOC;
            break;
        case 'b':
            rv.show_back = !rv.show_back;
            if (!rv.show_back && rv.focus == F_BACK)
                rv.focus = F_DOC;
            break;
        case 'u':
            if (rv.doc.buf) {
                rv.show_back = 1;
                rv.focus = F_BACK;
                rv.back_sel = 0;
            }
            break;
        case 'l':
            act_outgoing(&rv);
            break;
        case 'e':
            act_edit(&rv);
            break;
        case 'r':
            act_rep(&rv);
            break;
        case 'n':
            act_new(&rv);
            break;
        case 'a':
            act_annotate(&rv);
            break;
        case K_FN(KEY_DC):
            act_trash(&rv);
            break;
        case 't':
            act_broken(&rv);
            break;
        case ']': case '[':
            rv.focus = F_DOC;
            pending = key;
            break;
        case K_FN(KEY_BACKSPACE):
            go_back(&rv);
            break;
        case 27:
            if (rv.filter[0]) {
                rv.filter[0] = '\0';
                rebuild_list(&rv, 1);
                break;
            }
            /* fall through */
        case 16:                                        /* Ctrl+P, com a Phileas */
            key = menu_run(&rv);
            if (key > 0) {
                if (key == ']' || key == '\n' || key == K_FN(KEY_BACKSPACE))
                    rv.focus = F_DOC;
                pending = key;
            }
            break;
        case K_FN(KEY_F(1)):
            help();
            break;
        default:
            break;
        }
    }
out:
    if (trace_f)
        fclose(trace_f);
    doc_free(&rv.doc);
    back_free(&rv);
    free(rv.list);
    model_free(&rv.m);
}
