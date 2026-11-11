#include "fview.h"
#include "flat.h"
#include "fterm.h"
#include "launch.h"
#include "table.h"

#include "keys.h"
#include "utf8.h"
#include "util.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define K_FN(k) (KEY_FLAG_FN | (k))

typedef struct {
    Project *p;
    Flat     flat;
    char     filter[128];
    int      editing;       /* s'escriu el filtre */
    long     sel;
    long     top;
    char     msg[200];
    int      mode;          /* 0 arbre, 1 taula (outliner) */
    Table    tab;
    long     tsel, ttop;
    int      tcol;          /* columna actual */
    int      sortcol;       /* TC_NONE o la columna d'ordre */
    int      sortdesc;
} View;

/* ---------- dibuix ---------- */

/* Escriu s a (y, x) sense passar de maxw columnes; si s'ha de tallar, acaba en
 * «…». Retorna les columnes ocupades. */
static int put(int y, int x, const char *s, int maxw, attr_t a)
{
    size_t n = strlen(s), i = 0, end = 0;
    int w = 0, wend = 0;

    if (maxw <= 0)
        return 0;
    while (i < n) {
        wchar_t c;
        size_t k = utf8_decode(s, n, i, &c);
        int cw = c == UTF8_INVALID ? 1 : utf8_width(c, w);

        if (w + cw > maxw)
            break;
        w += cw;
        i += k ? k : 1;
        if (w <= maxw - 1) {
            end = i;
            wend = w;
        }
    }
    attron(a);
    if (i >= n) {
        mvaddnstr(y, x, s, (int)n);
    } else {
        mvaddnstr(y, x, s, (int)end);
        mvaddstr(y, x + wend, "…");
        w = wend + 1;
    }
    attroff(a);
    return w;
}

static int text_width(const char *s)
{
    size_t n = strlen(s), i = 0;
    int w = 0;

    while (i < n) {
        wchar_t c;
        size_t k = utf8_decode(s, n, i, &c);

        w += c == UTF8_INVALID ? 1 : utf8_width(c, w);
        i += k ? k : 1;
    }
    return w;
}

static void fill(int y, attr_t a)
{
    attron(a);
    move(y, 0);
    for (int x = 0; x < COLS; x++)
        addch(' ');
    attroff(a);
}

static void hrule(int y)
{
    attron(ft_attr(FT_DIM));
    move(y, 0);
    for (int x = 0; x < COLS; x++)
        addstr("─");
    attroff(ft_attr(FT_DIM));
}

static void header(const Project *p)
{
    char w[32], o[32], right[160], left[160], date[8] = "";
    long daily = project_daily_needed(p, time(NULL));
    int lw;

    fill(0, ft_attr(FT_BAR));
    snprintf(left, sizeof left, " %s", p->titol);
    fk_fmt_num(project_words(p), w, sizeof w);
    if (p->objectiu > 0) {
        fk_fmt_num((size_t)p->objectiu, o, sizeof o);
        snprintf(right, sizeof right, "%s / %s (%ld %%)", w, o,
                 (long)(100.0 * (double)project_words(p) / (double)p->objectiu + 0.5));
        if (p->data_limit[0] && strlen(p->data_limit) >= 10) {
            snprintf(date, sizeof date, "%.2s-%.2s", p->data_limit + 8, p->data_limit + 5);
            if (daily == 0)
                strncat(right, " · assolit", sizeof right - strlen(right) - 1);
            else if (daily > 0) {
                char extra[64];

                snprintf(extra, sizeof extra, " · %ld/dia fins %s", daily, date);
                strncat(right, extra, sizeof right - strlen(right) - 1);
            }
        }
    } else {
        snprintf(right, sizeof right, "%s paraules", w);
    }
    lw = text_width(left);
    if (lw + text_width(right) + 3 > COLS) {      /* sense el ritme diari */
        char *dot = strstr(right, " · ");

        if (dot)
            *dot = '\0';
    }
    if (lw + text_width(right) + 3 > COLS && p->objectiu > 0)
        snprintf(right, sizeof right, "%ld %%",
                 (long)(100.0 * (double)project_words(p) / (double)p->objectiu + 0.5));
    {
        int rw = text_width(right);

        put(0, 0, left, COLS - rw - 2, ft_attr(FT_BAR) | A_BOLD);
        if (rw + 1 < COLS)
            put(0, COLS - rw - 1, right, rw, ft_attr(FT_BAR));
    }
}

static void draw_row(const View *v, long i, int y)
{
    const FRow *r = &v->flat.rows[i];
    const FNode *n = r->node;
    int selected = i == v->sel, show_estat = COLS >= 50 && n->kind == FK_CHAPTER;
    int estat_w = show_estat ? 11 : 0, words_w = 8;
    int words_x = COLS - estat_w - words_w, x = 1 + 2 * r->depth, title_w;
    attr_t base = selected ? ft_attr(FT_SEL) : n->kind == FK_GROUP ? ft_attr(FT_HEAD) : 0;
    char buf[200], num[32];
    const char *mark = n->nkids ? (n->open || v->filter[0] ? "▾ " : "▸ ") : "  ";

    if (selected)
        fill(y, base);
    else
        attrset(A_NORMAL);
    if (x >= COLS)
        return;
    attron(base);
    mvaddstr(y, x, mark);
    attroff(base);
    x += 2;
    title_w = words_x - x - 1;
    if (title_w < 4)
        title_w = COLS - x - 1;
    if (n->kind == FK_GROUP && n != v->p->manuscrit)
        snprintf(buf, sizeof buf, "%s (%zu)", n->title, n->nkids);
    else
        snprintf(buf, sizeof buf, "%s", n->title);
    put(y, x, buf, title_w, base);
    if (n->kind != FK_GROUP || n == v->p->manuscrit) {
        fk_fmt_num(n->words, num, sizeof num);
        if (words_x > x + 4)
            put(y, words_x + words_w - 1 - text_width(num), num, words_w - 1, base);
    }
    if (show_estat && n->estat)
        put(y, COLS - estat_w + 1, n->estat, estat_w - 2,
            selected ? base : n->estat_set ? 0 : ft_attr(FT_DIM));
}

static FNode *cur(const View *v);

/* Sinopsi (o resum) de l'element seleccionat, en dues línies */
static void footer_detail(const View *v, int y0)
{
    const FNode *n = cur(v);
    char line[2][256];
    const char *s;
    size_t len;
    int maxw = COLS - 2;

    line[0][0] = line[1][0] = '\0';
    if (n && n->sinopsi && n->sinopsi[0]) {
        s = n->sinopsi;
        len = strlen(s);
        if (text_width(s) <= maxw) {
            snprintf(line[0], sizeof line[0], "%s", s);
        } else {
            /* talla per l'últim espai que hi cap */
            size_t cut = 0, i = 0;
            int w = 0;

            while (i < len) {
                wchar_t c;
                size_t k = utf8_decode(s, len, i, &c);

                w += c == UTF8_INVALID ? 1 : utf8_width(c, w);
                if (w > maxw)
                    break;
                if (s[i] == ' ')
                    cut = i;
                i += k ? k : 1;
            }
            if (!cut)
                cut = i;
            snprintf(line[0], sizeof line[0], "%.*s", (int)cut, s);
            snprintf(line[1], sizeof line[1], "%s", s + cut + (s[cut] == ' '));
        }
        put(y0, 1, line[0], maxw, 0);
        if (line[1][0])
            put(y0 + 1, 1, line[1], maxw, 0);
    } else if (n) {
        put(y0, 1, "(sense sinopsi)", maxw, ft_attr(FT_DIM));
    }
}

static void status(const View *v)
{
    char buf[256];

    fill(LINES - 1, ft_attr(FT_BAR));
    if (v->editing) {
        snprintf(buf, sizeof buf, " Filtra: %s_   (Retorn accepta · Esc esborra)", v->filter);
    } else if (v->msg[0]) {
        snprintf(buf, sizeof buf, " %s", v->msg);
    } else if (v->filter[0]) {
        snprintf(buf, sizeof buf, " Filtre «%s» · / canvia · Esc l'esborra", v->filter);
    } else if (v->mode) {
        snprintf(buf, sizeof buf, " Tab arbre · ←→ columna · o ordre · m edita · / filtra · F1 ajuda");
    } else {
        snprintf(buf, sizeof buf, " n nou · r reanomena · e estat · Tab taula · / filtra · F1 ajuda");
    }
    put(LINES - 1, 0, buf, COLS, ft_attr(FT_BAR));
}

static int list_rows(void)
{
    int h = (LINES >= 30 ? LINES - 5 : LINES - 2) - 2 + 1;

    return h < 1 ? 1 : h;
}

/* ---------- vista de taula (outliner) ---------- */

typedef struct {
    int col[TC_N];     /* x de cada columna, o -1 si no hi cap */
    int w[TC_N];
} Layout;

static Layout table_layout(void)
{
    static const int fixed[TC_N] = { 0, 10, 12, 10, 10, 10 };
    Layout l;
    int avail = COLS - 2, used = 0, x = 1;

    for (int c = 0; c < TC_N; c++)
        l.col[c] = -1, l.w[c] = 0;
    for (int c = TC_POV; c < TC_N; c++) {
        int drop = (c == TC_DATA && COLS < 70) || (c == TC_LLOC && COLS < 58) || (c == TC_POV && COLS < 44);

        if (!drop) {
            l.w[c] = fixed[c];
            used += fixed[c] + 1;
        }
    }
    l.w[TC_TITLE] = avail - used;
    if (l.w[TC_TITLE] < 8)
        l.w[TC_TITLE] = 8;
    for (int c = 0; c < TC_N; c++)
        if (l.w[c]) {
            l.col[c] = x;
            x += l.w[c] + 1;
        }
    return l;
}

static void table_header(const View *v, const Layout *l)
{
    int y = 2;

    fill(y, 0);
    for (int c = 0; c < TC_N; c++) {
        char name[48];

        if (l->col[c] < 0)
            continue;
        snprintf(name, sizeof name, "%s%s", table_col_name(c),
                 c == v->sortcol ? (v->sortdesc ? " ▼" : " ▲") : "");
        put(y, l->col[c], name, l->w[c],
            c == v->tcol ? ft_attr(FT_HEAD) | A_UNDERLINE : ft_attr(FT_DIM));
    }
}

static void table_row(const View *v, long i, int y, const Layout *l)
{
    const TRow *r = &v->tab.rows[i];
    const FNode *n = r->node;
    int selected = i == v->tsel;
    char buf[300];

    if (selected)
        fill(y, ft_attr(FT_SEL));
    for (int c = 0; c < TC_N; c++) {
        attr_t a = selected ? ft_attr(FT_SEL) : 0;
        int x, w, tw;
        int own = 1;

        if (l->col[c] < 0)
            continue;
        table_cell(&v->tab, n, c, buf, sizeof buf);
        x = l->col[c];
        w = l->w[c];
        if (c == TC_TITLE) {
            x += 2 * r->depth;
            w -= 2 * r->depth;
            if (n->kind == FK_CHAPTER && !selected)
                a = ft_attr(FT_HEAD);
        } else if (n->kind == FK_SCENE) {
            own = c == TC_POV ? n->own & FK_OWN_POV : c == TC_LLOC ? n->own & FK_OWN_LLOC :
                  c == TC_DATA ? n->own & FK_OWN_DATA : c == TC_ESTAT ? (unsigned)n->estat_set : 1u;
            if (!own && !selected)
                a = ft_attr(FT_DIM);                  /* heretat del capítol */
        }
        if (c == v->tcol)
            a |= A_UNDERLINE;
        if (c == TC_WORDS) {
            tw = text_width(buf);
            put(y, x + w - tw, buf, tw < w ? tw : w, a);
        } else {
            put(y, x, buf, w, a);
        }
    }
}

static int table_body_rows(void)
{
    return list_rows() - 1 < 1 ? 1 : list_rows() - 1;
}

static void paint_table(View *v)
{
    Layout l = table_layout();
    int h = table_body_rows();

    erase();
    header(v->p);
    hrule(1);
    if (v->tsel >= (long)v->tab.n)
        v->tsel = (long)v->tab.n - 1;
    if (v->tsel < 0)
        v->tsel = 0;
    if (v->tsel < v->ttop)
        v->ttop = v->tsel;
    if (v->tsel >= v->ttop + h)
        v->ttop = v->tsel - h + 1;
    if (v->ttop < 0)
        v->ttop = 0;
    table_header(v, &l);
    if (!v->tab.n) {
        put(4, 2, v->filter[0] ? "Cap resultat." : "El manuscrit és buit.", COLS - 3, ft_attr(FT_DIM));
    } else {
        for (int r = 0; r < h && v->ttop + r < (long)v->tab.n; r++)
            table_row(v, v->ttop + r, 3 + r, &l);
    }
    if (LINES >= 30) {
        hrule(LINES - 4);
        footer_detail(v, LINES - 3);
    }
    status(v);
}

/* Pinta tota la pantalla al buffer de ncurses, sense enviar-la al terminal */
static void paint(View *v)
{
    int h = list_rows();

    if (v->mode) {
        paint_table(v);
        return;
    }
    erase();
    header(v->p);
    hrule(1);
    if (v->sel >= (long)v->flat.n)
        v->sel = (long)v->flat.n - 1;
    if (v->sel < 0)
        v->sel = 0;
    if (v->sel < v->top)
        v->top = v->sel;
    if (v->sel >= v->top + h)
        v->top = v->sel - h + 1;
    if (v->top < 0)
        v->top = 0;
    if (!v->flat.n) {
        put(3, 2, v->filter[0] ? "Cap resultat." : "El projecte és buit.", COLS - 3, ft_attr(FT_DIM));
    } else {
        for (int r = 0; r < h && v->top + r < (long)v->flat.n; r++)
            draw_row(v, v->top + r, 2 + r);
    }
    if (LINES >= 30) {
        hrule(LINES - 4);
        footer_detail(v, LINES - 3);
    }
    status(v);
}

static void draw(View *v)
{
    paint(v);
    refresh();
}

/* ---------- accions ---------- */

static void rebuild(View *v, const FNode *keep)
{
    long k;

    flat_free(&v->flat);
    flat_build(&v->flat, v->p, v->filter);
    k = keep ? flat_find(&v->flat, keep) : -1;
    if (k >= 0)
        v->sel = k;
    if (v->sel >= (long)v->flat.n)
        v->sel = (long)v->flat.n - 1;
    if (v->sel < 0)
        v->sel = 0;

    table_free(&v->tab);
    table_build(&v->tab, v->p, v->filter, v->sortcol, v->sortdesc);
    k = keep ? table_find(&v->tab, keep) : -1;
    if (k < 0 && keep && keep->kind == FK_CHAPTER && keep->nkids)
        k = table_find(&v->tab, keep->kids[0]);
    if (k >= 0)
        v->tsel = k;
    if (v->tsel >= (long)v->tab.n)
        v->tsel = (long)v->tab.n - 1;
    if (v->tsel < 0)
        v->tsel = 0;
}

static FNode *cur(const View *v)
{
    if (v->mode)
        return v->tab.n ? v->tab.rows[v->tsel].node : NULL;
    return v->flat.n ? v->flat.rows[v->sel].node : NULL;
}

static void toggle(View *v, FNode *n)
{
    if (n->nkids) {
        n->open = !n->open;
        rebuild(v, n);
    }
}

static int confirm(const char *question);
static void refresh_select(View *v, const char *path);

/* Instantània automàtica abans de tocar un fitxer (com a molt una per hora);
 * si falla, només es mostra l'avís: no atura el que l'usuari volia fer. */
static void auto_snapshot(View *v, FNode *n)
{
    char err[200];

    if (n && (n->kind == FK_CHAPTER || n->kind == FK_CARD) &&
        project_snapshot(v->p, n, 0, time(NULL), err, sizeof err) < 0)
        snprintf(v->msg, sizeof v->msg, "Sense instantània: %.150s", err);
}

static void act_snapshot(View *v)
{
    FNode *n = cur(v);
    char err[200];
    int rc;

    if (n && n->kind == FK_SCENE)
        n = n->parent;
    if (!n || (n->kind != FK_CHAPTER && n->kind != FK_CARD)) {
        snprintf(v->msg, sizeof v->msg, "Les instantànies són de capítols i fitxes.");
        return;
    }
    rc = project_snapshot(v->p, n, 1, time(NULL), err, sizeof err);
    if (rc < 0)
        snprintf(v->msg, sizeof v->msg, "%s", err);
    else if (rc == 1)
        snprintf(v->msg, sizeof v->msg, "Ja hi ha una instantània idèntica a l'actual.");
    else
        snprintf(v->msg, sizeof v->msg, "Instantània de «%s» desada (v per veure-les)", n->title);
}

/* Llista de versions; retorna l'índex triat o -1 */
static int versions_dialog(View *v, const FNode *n, const FVersion *ver, size_t cnt)
{
    char head[200], row[120], num[32];
    long sel = 0;
    int w = 52;

    snprintf(head, sizeof head, " Versions de «%s» ", n->title);
    if (text_width(head) + 2 > w)
        w = text_width(head) + 2;
    if (w > COLS - 2)
        w = COLS - 2;
    for (;;) {
        int x0 = 1, y0 = 2, key, visible = LINES - 6 < (int)cnt ? LINES - 6 : (int)cnt;
        long top = sel >= visible ? sel - visible + 1 : 0;

        if (visible < 1)
            visible = 1;
        paint(v);
        attrset(A_NORMAL);
        attron(ft_attr(FT_DIM));
        mvaddstr(y0, x0, "┌");
        for (int c = 0; c < w; c++)
            addstr("─");
        addstr("┐");
        attroff(ft_attr(FT_DIM));
        put(y0, x0 + 1, head, w, ft_attr(FT_HEAD));
        for (int r = 0; r < visible; r++) {
            const FVersion *e = &ver[top + r];
            struct tm tm;
            char when[32];
            attr_t a = top + r == sel ? ft_attr(FT_SEL) : 0;

            localtime_r(&e->when, &tm);
            strftime(when, sizeof when, "%Y-%m-%d %H:%M:%S", &tm);
            fk_fmt_num(e->words, num, sizeof num);
            snprintf(row, sizeof row, "%s  %8s paraules  (%+ld)", when, num,
                     (long)e->words - (long)n->words);
            attron(ft_attr(FT_DIM));
            mvaddstr(y0 + 1 + r, x0, "│");
            mvaddstr(y0 + 1 + r, x0 + w + 1, "│");
            attroff(ft_attr(FT_DIM));
            attron(a);
            move(y0 + 1 + r, x0 + 1);
            for (int c = 0; c < w; c++)
                addch(' ');
            attroff(a);
            put(y0 + 1 + r, x0 + 2, row, w - 2, a);
        }
        attron(ft_attr(FT_DIM));
        mvaddstr(y0 + 1 + visible, x0, "└");
        for (int c = 0; c < w; c++)
            addstr("─");
        addstr("┘");
        attroff(ft_attr(FT_DIM));
        put(y0 + 2 + visible, x0, " Retorn recupera la versió · Esc tanca", COLS - 2, ft_attr(FT_DIM));
        refresh();
        key = key_read();
        if (key < 0 || key == K_FN(KEY_RESIZE))
            continue;
        if (key == 27 || key == 'q')
            return -1;
        if (key == K_FN(KEY_UP) && sel > 0)
            sel--;
        else if (key == K_FN(KEY_DOWN) && sel + 1 < (long)cnt)
            sel++;
        else if (key == K_FN(KEY_HOME))
            sel = 0;
        else if (key == K_FN(KEY_END))
            sel = (long)cnt - 1;
        else if (key == K_FN(KEY_PPAGE))
            sel = sel > 8 ? sel - 8 : 0;
        else if (key == K_FN(KEY_NPAGE))
            sel = sel + 8 < (long)cnt ? sel + 8 : (long)cnt - 1;
        else if (key == '\n')
            return (int)sel;
    }
}

static void act_versions(View *v)
{
    FNode *n = cur(v);
    FVersion *ver;
    size_t cnt;
    int pick;
    char path[600], q[300], err[200], when[32];
    struct tm tm;

    if (n && n->kind == FK_SCENE)
        n = n->parent;
    if (!n || (n->kind != FK_CHAPTER && n->kind != FK_CARD)) {
        snprintf(v->msg, sizeof v->msg, "Les versions són de capítols i fitxes.");
        return;
    }
    cnt = project_versions(v->p, n, &ver);
    if (!cnt) {
        snprintf(v->msg, sizeof v->msg, "Cap versió d'aquest fitxer; amb s en pots desar una.");
        return;
    }
    pick = versions_dialog(v, n, ver, cnt);
    if (pick >= 0) {
        localtime_r(&ver[pick].when, &tm);
        strftime(when, sizeof when, "%Y-%m-%d %H:%M:%S", &tm);
        snprintf(q, sizeof q, "Recuperar la versió de %s? L'estat actual es desa abans.", when);
        if (confirm(q)) {
            snprintf(path, sizeof path, "%s", n->path);
            if (project_restore(v->p, n, &ver[pick], time(NULL), err, sizeof err) != 0)
                snprintf(v->msg, sizeof v->msg, "%s", err);
            else {
                refresh_select(v, path);
                if (!v->msg[0])
                    snprintf(v->msg, sizeof v->msg, "Versió de %s recuperada", when);
            }
        }
    }
    project_versions_free(ver, cnt);
}

/* Obre l'element a l'editor i, en tornar, refresca el projecte conservant la
 * selecció (pel camí del fitxer i l'ordre de l'escena). */
static void open_in_editor(View *v, FNode *n)
{
    FNode *file = n->kind == FK_SCENE ? n->parent : n;
    size_t line = n->kind == FK_SCENE ? n->line + 1 : 0, scene = 0;
    char *path = strdup(file->path), err[200];
    int rc;

    if (!path)
        return;
    if (n->kind == FK_SCENE)
        for (size_t i = 0; i < file->nkids; i++)
            if (file->kids[i] == n)
                scene = i + 1;
    auto_snapshot(v, file);
    v->msg[0] = '\0';
    rc = launch_editor(path, line, err, sizeof err);
    if (project_refresh(v->p, v->msg, sizeof v->msg) == 0) {
        FNode *keep = NULL;

        for (size_t g = 0; g < v->p->top->nkids && !keep; g++)
            for (size_t i = 0; i < v->p->top->kids[g]->nkids; i++) {
                FNode *c = v->p->top->kids[g]->kids[i];

                if (!strcmp(c->path, path)) {
                    keep = scene && scene <= c->nkids ? c->kids[scene - 1] : c;
                    if (keep != c && c->nkids)
                        c->open = 1;
                    break;
                }
            }
        rebuild(v, keep);
    } else {
        flat_free(&v->flat);
        flat_build(&v->flat, v->p, v->filter);
    }
    if (rc != 0)
        snprintf(v->msg, sizeof v->msg, "%s", err);
    free(path);
}

/* Línia d'entrada a la barra d'estat. Retorna 1 si s'accepta (Retorn) i 0 si
 * es cancel·la (Esc); buf conserva el text. */
static int prompt_line(const char *label, const char *initial, char *buf, size_t sz)
{
    size_t l;

    snprintf(buf, sz, "%s", initial ? initial : "");
    l = strlen(buf);
    for (;;) {
        char shown[400];
        int col;
        int key;

        snprintf(shown, sizeof shown, " %s%s", label, buf);
        fill(LINES - 1, ft_attr(FT_BAR));
        put(LINES - 1, 0, shown, COLS, ft_attr(FT_BAR));
        col = text_width(shown);
        curs_set(1);
        move(LINES - 1, col < COLS ? col : COLS - 1);
        refresh();
        key = key_read();
        if (key < 0 || key == K_FN(KEY_RESIZE))
            continue;
        if (key == '\n') {
            curs_set(0);
            return 1;
        } else if (key == 27) {
            curs_set(0);
            return 0;
        } else if (key == 21) {                         /* Ctrl+U */
            buf[l = 0] = '\0';
        } else if (key == K_FN(KEY_BACKSPACE) || key == 127 || key == 8) {
            while (l && ((unsigned char)buf[l - 1] & 0xC0) == 0x80)
                l--;
            if (l)
                buf[--l] = '\0';
        } else if (key >= 32 && !(key & (KEY_FLAG_FN | KEY_FLAG_ALT))) {
            char enc[4];
            size_t k = utf8_encode((wchar_t)key, enc);

            if (l + k < sz) {
                memcpy(buf + l, enc, k);
                buf[l += k] = '\0';
            }
        }
    }
}

static int confirm(const char *question)
{
    char shown[400];
    int key;

    snprintf(shown, sizeof shown, " %s (s/n) ", question);
    fill(LINES - 1, ft_attr(FT_BAR));
    put(LINES - 1, 0, shown, COLS, ft_attr(FT_BAR));
    refresh();
    do
        key = key_read();
    while (key < 0 || key == K_FN(KEY_RESIZE));
    return key == 's' || key == 'S' || key == 'y' || key == 'Y';
}

/* Torna a llegir el projecte després d'un canvi als fitxers i selecciona el
 * fitxer del camí (si n'hi ha), desplegant-ne el grup. */
/* Torna a llegir el projecte i selecciona el fitxer del camí i, si scene > 0,
 * la seva escena número scene (1 = la primera) */
static void refresh_select_scene(View *v, const char *path, size_t scene)
{
    FNode *keep = NULL;
    char err[200];

    if (project_refresh(v->p, err, sizeof err) != 0) {
        snprintf(v->msg, sizeof v->msg, "%s", err);
        flat_free(&v->flat);
        flat_build(&v->flat, v->p, v->filter);
        return;
    }
    for (size_t g = 0; path && g < v->p->top->nkids && !keep; g++)
        for (size_t i = 0; i < v->p->top->kids[g]->nkids; i++) {
            FNode *c = v->p->top->kids[g]->kids[i];

            if (!strcmp(c->path, path)) {
                keep = scene && scene <= c->nkids ? c->kids[scene - 1] : c;
                v->p->top->kids[g]->open = 1;
                if (keep != c)
                    c->open = 1;
                break;
            }
        }
    rebuild(v, keep);
}

static void refresh_select(View *v, const char *path)
{
    refresh_select_scene(v, path, 0);
}

static FNode *group_of(const FNode *n)
{
    if (!n)
        return NULL;
    if (n->kind == FK_GROUP)
        return (FNode *)n;
    if (n->kind == FK_SCENE)
        n = n->parent;
    return n && n->parent && n->parent->kind == FK_GROUP ? n->parent : NULL;
}

static void act_new(View *v)
{
    FNode *g = group_of(cur(v));
    char title[200], label[120], path[600], err[200];
    int chapter = !g || g == v->p->manuscrit;

    snprintf(label, sizeof label, chapter ? "Títol del nou capítol: " : "Títol de la nova fitxa (%s): ",
             g ? g->title : "");
    if (!prompt_line(label, "", title, sizeof title))
        return;
    if (project_new_item(v->p, g, title, path, sizeof path, err, sizeof err) != 0) {
        snprintf(v->msg, sizeof v->msg, "%s", err);
        return;
    }
    refresh_select(v, path);
    if (!v->msg[0])
        snprintf(v->msg, sizeof v->msg, "Creat %s · Retorn per obrir-lo", strrchr(path, '/') + 1);
}

static void act_rename(View *v)
{
    FNode *n = cur(v);
    char title[200], path[600], err[200], oldp[600];

    if (!n || (n->kind != FK_CHAPTER && n->kind != FK_CARD)) {
        snprintf(v->msg, sizeof v->msg, n && n->kind == FK_SCENE ?
                 "Les escenes es reanomenen a Phileas (la capçalera ###)." :
                 "Només es reanomenen capítols i fitxes.");
        return;
    }
    if (!prompt_line("Títol nou: ", n->title, title, sizeof title))
        return;
    snprintf(oldp, sizeof oldp, "%s", n->path);
    auto_snapshot(v, n);
    if (project_rename(v->p, n, title, path, sizeof path, err, sizeof err) != 0) {
        snprintf(v->msg, sizeof v->msg, "%s", err);
        refresh_select(v, oldp);
        snprintf(v->msg, sizeof v->msg, "%s", err);
        return;
    }
    refresh_select(v, path);
}

static void act_trash(View *v)
{
    FNode *n = cur(v);
    char q[300], err[200];
    long keep;

    if (!n || (n->kind != FK_CHAPTER && n->kind != FK_CARD)) {
        snprintf(v->msg, sizeof v->msg, "Només es mouen a la paperera capítols i fitxes.");
        return;
    }
    snprintf(q, sizeof q, "Moure «%s» a la paperera?", n->title);
    if (!confirm(q))
        return;
    keep = v->sel;
    if (project_trash(v->p, n, err, sizeof err) != 0) {
        snprintf(v->msg, sizeof v->msg, "%s", err);
        return;
    }
    refresh_select(v, NULL);
    v->sel = keep < (long)v->flat.n ? keep : (long)v->flat.n - 1;
    if (v->sel < 0)
        v->sel = 0;
    if (!v->msg[0])
        snprintf(v->msg, sizeof v->msg, "Mogut a .paperera/ (no s'ha esborrat)");
}

static void act_move(View *v, int dir)
{
    FNode *n = cur(v);
    char path[600], err[200];

    if (!n || n->kind != FK_CHAPTER) {
        snprintf(v->msg, sizeof v->msg, n && n->kind == FK_SCENE ?
                 "Les escenes es mouen a Phileas (Alt+↑/↓ a l'esquema, F2)." :
                 "Només es reordenen els capítols.");
        return;
    }
    if (v->filter[0]) {
        snprintf(v->msg, sizeof v->msg, "Esborra el filtre (Esc) abans de reordenar.");
        return;
    }
    if (project_move(v->p, n, dir, path, sizeof path, err, sizeof err) != 0) {
        snprintf(v->msg, sizeof v->msg, "%s", err);
        return;
    }
    refresh_select(v, path);
}

/* Menú de l'Esc: la mateixa acció que cada tecla. Retorna el codi de la tecla
 * de l'acció triada, o -1 si es tanca sense triar. */
static int menu_run(View *v)
{
    static const struct { const char *label, *hint; int key; } items[] = {
        { "Obre a Phileas", "Retorn", '\n' },
        { "Canvia de vista (arbre/taula)", "Tab", 9 },
        { "Desplega o plega", "Espai", ' ' },
        { "Nou capítol o fitxa", "n", 'n' },
        { "Canvia el títol", "r", 'r' },
        { "Canvia l'estat", "e", 'e' },
        { "Puja el capítol", "Alt+↑", '<' },
        { "Baixa el capítol", "Alt+↓", '>' },
        { "Desa una instantània", "s", 's' },
        { "Versions anteriors", "v", 'v' },
        { "Mou a la paperera", "Supr", K_FN(KEY_DC) },
        { "Filtra", "/", '/' },
        { "Ajuda", "F1", K_FN(KEY_F(1)) },
        { "Surt", "Ctrl+Q", 17 },
    };
    const int n = (int)(sizeof items / sizeof *items);
    int sel = 0, w = 0;

    for (int i = 0; i < n; i++) {
        int lw = text_width(items[i].label) + 2 + text_width(items[i].hint) + 4;

        if (lw > w)
            w = lw;
    }
    if (w > COLS - 2)
        w = COLS - 2;
    for (;;) {
        int x0 = 1, y0 = 2, key;

        paint(v);
        attrset(A_NORMAL);
        for (int i = -1; i <= n; i++) {
            int y = y0 + 1 + i;

            if (y >= LINES - 1)
                break;
            if (i < 0 || i == n) {
                attron(ft_attr(FT_DIM));
                mvaddstr(y, x0, i < 0 ? "┌" : "└");
                for (int c = 0; c < w; c++)
                    addstr("─");
                addstr(i < 0 ? "┐" : "┘");
                attroff(ft_attr(FT_DIM));
            } else {
                attr_t a = i == sel ? ft_attr(FT_SEL) : 0;
                int lw = text_width(items[i].label), hw = text_width(items[i].hint);

                attron(ft_attr(FT_DIM));
                mvaddstr(y, x0, "│");
                mvaddstr(y, x0 + w + 1, "│");
                attroff(ft_attr(FT_DIM));
                attron(a);
                move(y, x0 + 1);
                for (int c = 0; c < w; c++)
                    addch(' ');
                attroff(a);
                put(y, x0 + 2, items[i].label, w - 2, a);
                put(y, x0 + w - hw - 1, items[i].hint, hw, a ? a : ft_attr(FT_DIM));
                (void)lw;
            }
        }
        refresh();
        key = key_read();
        if (key < 0 || key == K_FN(KEY_RESIZE))
            continue;
        if (key == 27 || key == K_FN(KEY_F(10)))
            return -1;
        if (key == K_FN(KEY_UP))
            sel = (sel + n - 1) % n;
        else if (key == K_FN(KEY_DOWN))
            sel = (sel + 1) % n;
        else if (key == K_FN(KEY_HOME))
            sel = 0;
        else if (key == K_FN(KEY_END))
            sel = n - 1;
        else if (key == '\n')
            return items[sel].key;
    }
}

static void help(void)
{
    static const char *const lines[] = {
        "Fàbula · ajuda",
        "",
        "  ↑ ↓ RePàg AvPàg Inici Fi   moure's",
        "  → / Espai                  desplegar (o entrar al primer fill)",
        "  ←                          plegar (o anar al pare)",
        "  Retorn                     grups: plega/desplega · la resta: obre a Phileas",
        "  n                          capítol nou (al manuscrit) o fitxa nova (al grup)",
        "  r                          canvia el títol (i el nom del fitxer)",
        "  s / v                      desa una instantània / veu les versions i en recupera una",
        "  Supr                       mou a la paperera (.paperera/); no s'esborra",
        "  Alt+↑ / Alt+↓  (< / >)     puja o baixa el capítol",
        "  e                          canvia l'estat del capítol o la fitxa",
        "  /                          filtra pel títol o la sinopsi",
        "  Tab                        canvia entre l'arbre i la taula (outliner)",
        "  Taula: ← →  columna · o  ordena (▲ ▼ i res) · m  edita la metadada de la columna",
        "  Esc                        esborra el filtre; sense filtre, el menú",
        "  Ctrl+L                     redibuixa",
        "  Ctrl+Q / q                 surt",
        "",
        "  Premeu una tecla per tornar.",
    };

    erase();
    for (size_t i = 0; i < sizeof lines / sizeof *lines && (int)i < LINES; i++)
        put((int)i, 1, lines[i], COLS - 2, i == 0 ? ft_attr(FT_HEAD) : 0);
    refresh();
    key_read();
}

static size_t scene_ordinal(const FNode *n)
{
    if (n->kind != FK_SCENE || !n->parent)
        return 0;
    for (size_t i = 0; i < n->parent->nkids; i++)
        if (n->parent->kids[i] == n)
            return i + 1;
    return 0;
}

/* Canvia un camp de metadades de l'element actual (capítol, escena o fitxa) i
 * refresca; value buit el treu. */
static void set_meta_current(View *v, FNode *n, const char *key, const char *value)
{
    FNode *file = n->kind == FK_SCENE ? n->parent : n;
    size_t scene = scene_ordinal(n);
    char path[600], err[240];

    snprintf(path, sizeof path, "%s", file->path);
    auto_snapshot(v, file);
    v->msg[0] = '\0';
    if (project_set_meta(v->p, n, key, value, err, sizeof err) != 0) {
        snprintf(v->msg, sizeof v->msg, "%.190s", err);
        return;
    }
    refresh_select_scene(v, path, scene);
}

static void change_estat(View *v)
{
    FNode *n = cur(v);
    const char *next;
    char what[200];

    if (!n || (n->kind != FK_CHAPTER && n->kind != FK_CARD && n->kind != FK_SCENE)) {
        snprintf(v->msg, sizeof v->msg, "L'estat només es canvia en capítols, escenes i fitxes.");
        return;
    }
    next = project_next_estat(v->p, n->estat);
    if (!next)
        return;
    snprintf(what, sizeof what, "%s: %s", n->title, next);
    set_meta_current(v, n, "estat", next);
    if (!v->msg[0])
        snprintf(v->msg, sizeof v->msg, "%s", what);
}

/* m: edita la metadada de la columna actual de la taula */
static void edit_column(View *v)
{
    FNode *n = cur(v);
    const char *key = table_col_key(v->tcol);
    char value[200], label[80], cell[200];

    if (!n)
        return;
    if (v->tcol == TC_ESTAT) {
        change_estat(v);
        return;
    }
    if (!key) {
        snprintf(v->msg, sizeof v->msg, v->tcol == TC_TITLE ?
                 "El títol es canvia amb r." : "Aquesta columna es calcula, no s'edita.");
        return;
    }
    if (n->kind == FK_GROUP) {
        return;
    }
    table_cell(&v->tab, n, v->tcol, cell, sizeof cell);
    snprintf(label, sizeof label, "%s de «%.40s» (buit = treure'l): ", table_col_name(v->tcol), n->title);
    if (!prompt_line(label, cell, value, sizeof value))
        return;
    set_meta_current(v, n, key, value);
}

static void filter_key(View *v, int key)
{
    size_t l = strlen(v->filter);

    if (key == '\n') {
        v->editing = 0;
    } else if (key == 27) {
        v->filter[0] = '\0';
        v->editing = 0;
        rebuild(v, cur(v));
    } else if (key == K_FN(KEY_BACKSPACE) || key == 127 || key == 8) {
        while (l && ((unsigned char)v->filter[l - 1] & 0xC0) == 0x80)
            l--;
        if (l)
            v->filter[l - 1] = '\0';
        rebuild(v, NULL);
        v->sel = 0;
    } else if (key >= 32 && !(key & (KEY_FLAG_FN | KEY_FLAG_ALT)) && key != 127) {
        char enc[4];
        size_t k = utf8_encode((wchar_t)key, enc);

        if (l + k < sizeof v->filter) {
            memcpy(v->filter + l, enc, k);
            v->filter[l + k] = '\0';
            rebuild(v, NULL);
            v->sel = 0;
        }
    }
}

static void toggle_mode(View *v)
{
    FNode *n = cur(v);

    v->mode = !v->mode;
    if (!v->mode && n) {                       /* a l'arbre, s'ha de poder veure */
        FNode *g = group_of(n);

        if (n->kind == FK_SCENE)
            n->parent->open = 1;
        if (g)
            g->open = 1;
    }
    rebuild(v, n);
}

/* Columna visible més pròxima en la direcció dir */
static void move_column(View *v, int dir)
{
    Layout l = table_layout();

    for (int c = v->tcol + dir; c >= 0 && c < TC_N; c += dir)
        if (l.col[c] >= 0) {
            v->tcol = c;
            return;
        }
}

/* Tecles de la taula; retorna 1 si les ha gestionades */
static int table_key(View *v, int key)
{
    long page = table_body_rows() - 1, n = (long)v->tab.n;
    Layout l = table_layout();
    FNode *node = cur(v);

    if (l.col[v->tcol] < 0)
        v->tcol = TC_TITLE;
    switch (key) {
    case K_FN(KEY_UP):    if (v->tsel > 0) v->tsel--; return 1;
    case K_FN(KEY_DOWN):  if (v->tsel + 1 < n) v->tsel++; return 1;
    case K_FN(KEY_PPAGE): v->tsel = v->tsel > page ? v->tsel - page : 0; return 1;
    case K_FN(KEY_NPAGE): v->tsel = v->tsel + page < n ? v->tsel + page : n - 1; return 1;
    case K_FN(KEY_HOME):  v->tsel = 0; return 1;
    case K_FN(KEY_END):   v->tsel = n - 1; return 1;
    case K_FN(KEY_LEFT):  move_column(v, -1); return 1;
    case K_FN(KEY_RIGHT): case ' ': move_column(v, 1); return 1;
    case 'o':
        if (v->sortcol != v->tcol) {
            v->sortcol = v->tcol;
            v->sortdesc = 0;
        } else if (!v->sortdesc) {
            v->sortdesc = 1;
        } else {
            v->sortcol = TC_NONE;
            v->sortdesc = 0;
        }
        rebuild(v, node);
        return 1;
    case 'm':
        edit_column(v);
        return 1;
    case '\n':
        if (node)
            open_in_editor(v, node);
        return 1;
    default:
        return 0;
    }
}

void fview_run(Project *p)
{
    View v;
    int pending = 0;

    memset(&v, 0, sizeof v);
    v.p = p;
    v.sortcol = TC_NONE;
    rebuild(&v, NULL);
    for (;;) {
        int key, page;
        FNode *n;

        ft_sync_size();
        draw(&v);
        if (pending) {
            key = pending;
            pending = 0;
        } else {
            key = key_read();
        }
        if (key < 0 || key == K_FN(KEY_RESIZE))
            continue;
        v.msg[0] = '\0';
        if (v.editing) {
            filter_key(&v, key);
            continue;
        }
        page = list_rows() - 1;
        n = cur(&v);
        if (v.mode && table_key(&v, key))
            continue;
        switch (key) {
        case 17: case 'q':                 /* Ctrl+Q */
            flat_free(&v.flat);
            table_free(&v.tab);
            return;
        case 9:                            /* Tab */
            toggle_mode(&v);
            break;
        case 12:                           /* Ctrl+L */
            clearok(curscr, TRUE);
            break;
        case K_FN(KEY_UP):    if (v.sel > 0) v.sel--; break;
        case K_FN(KEY_DOWN):  if (v.sel + 1 < (long)v.flat.n) v.sel++; break;
        case K_FN(KEY_PPAGE): v.sel = v.sel > page ? v.sel - page : 0; break;
        case K_FN(KEY_NPAGE): v.sel = v.sel + page < (long)v.flat.n ? v.sel + page : (long)v.flat.n - 1; break;
        case K_FN(KEY_HOME):  v.sel = 0; break;
        case K_FN(KEY_END):   v.sel = (long)v.flat.n - 1; break;
        case K_FN(KEY_RIGHT): case ' ':
            if (n && n->nkids) {
                if (!n->open && !v.filter[0])
                    toggle(&v, n);
                else if (v.sel + 1 < (long)v.flat.n)
                    v.sel++;
            }
            break;
        case K_FN(KEY_LEFT):
            if (n && n->nkids && n->open && !v.filter[0]) {
                toggle(&v, n);
            } else if (n && n->parent && n->parent->kind != FK_ROOT) {
                long k = flat_find(&v.flat, n->parent);

                if (k >= 0)
                    v.sel = k;
            }
            break;
        case '\n':
            if (n && n->kind == FK_GROUP)
                toggle(&v, n);
            else if (n)
                open_in_editor(&v, n);
            break;
        case 'e':
            change_estat(&v);
            break;
        case 'n':
            act_new(&v);
            break;
        case 's':
            act_snapshot(&v);
            break;
        case 'v':
            act_versions(&v);
            break;
        case 'r':
            act_rename(&v);
            break;
        case K_FN(KEY_DC):
            act_trash(&v);
            break;
        case KEY_MODIFIED(KB_UP, 3): case '<':
            act_move(&v, -1);
            break;
        case KEY_MODIFIED(KB_DOWN, 3): case '>':
            act_move(&v, 1);
            break;
        case '/':
            v.editing = 1;
            break;
        case 27:
            if (v.filter[0]) {
                v.filter[0] = '\0';
                rebuild(&v, n);
                break;
            }
            key = menu_run(&v);
            if (key < 0)
                break;
            pending = key;
            break;
        case K_FN(KEY_F(1)):
            help();
            break;
        default:
            break;
        }
    }
}
