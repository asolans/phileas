#include "fview.h"
#include "flat.h"
#include "fterm.h"
#include "compile.h"
#include "diary.h"
#include "find.h"
#include "launch.h"
#include "ui.h"
#include "table.h"
#include "timeline.h"
#include "yaml.h"

#include "keys.h"
#include "utf8.h"
#include "util.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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

    cm_blank(0, 0, COLS, ft_attr(FT_BAR));
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
    lw = cm_width(left);
    if (lw + cm_width(right) + 3 > COLS) {      /* sense el ritme diari */
        char *dot = strstr(right, " · ");

        if (dot)
            *dot = '\0';
    }
    if (lw + cm_width(right) + 3 > COLS && p->objectiu > 0)
        snprintf(right, sizeof right, "%ld %%",
                 (long)(100.0 * (double)project_words(p) / (double)p->objectiu + 0.5));
    {
        int rw = cm_width(right);

        cm_put(0, 0, left, COLS - rw - 2, ft_attr(FT_BAR) | A_BOLD);
        if (rw + 1 < COLS)
            cm_put(0, COLS - rw - 1, right, rw, ft_attr(FT_BAR));
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
        cm_blank(y, 0, COLS, base);
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
    cm_put(y, x, buf, title_w, base);
    if (n->kind != FK_GROUP || n == v->p->manuscrit) {
        fk_fmt_num(n->words, num, sizeof num);
        if (words_x > x + 4)
            cm_put(y, words_x + words_w - 1 - cm_width(num), num, words_w - 1, base);
    }
    if (show_estat && n->estat)
        cm_put(y, COLS - estat_w + 1, n->estat, estat_w - 2,
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
        if (cm_width(s) <= maxw) {
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
        cm_put(y0, 1, line[0], maxw, 0);
        if (line[1][0])
            cm_put(y0 + 1, 1, line[1], maxw, 0);
    } else if (n) {
        cm_put(y0, 1, "(sense sinopsi)", maxw, ft_attr(FT_DIM));
    }
}

static void status(const View *v)
{
    char buf[256];

    cm_blank(LINES - 1, 0, COLS, ft_attr(FT_BAR));
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
    cm_put(LINES - 1, 0, buf, COLS, ft_attr(FT_BAR));
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

    cm_blank(y, 0, COLS, 0);
    for (int c = 0; c < TC_N; c++) {
        char name[48];

        if (l->col[c] < 0)
            continue;
        snprintf(name, sizeof name, "%s%s", table_col_name(c),
                 c == v->sortcol ? (v->sortdesc ? " ▼" : " ▲") : "");
        cm_put(y, l->col[c], name, l->w[c],
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
        cm_blank(y, 0, COLS, ft_attr(FT_SEL));
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
            tw = cm_width(buf);
            cm_put(y, x + w - tw, buf, tw < w ? tw : w, a);
        } else {
            cm_put(y, x, buf, w, a);
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
        cm_put(4, 2, v->filter[0] ? "Cap resultat." : "El manuscrit és buit.", COLS - 3, ft_attr(FT_DIM));
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
        cm_put(3, 2, v->filter[0] ? "Cap resultat." : "El projecte és buit.", COLS - 3, ft_attr(FT_DIM));
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

/* paint() per als diàlegs de comu/ui */
static void paint_cb(void *ud)
{
    paint(ud);
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
    if (cm_width(head) + 2 > w)
        w = cm_width(head) + 2;
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
        cm_put(y0, x0 + 1, head, w, ft_attr(FT_HEAD));
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
            cm_put(y0 + 1 + r, x0 + 2, row, w - 2, a);
        }
        attron(ft_attr(FT_DIM));
        mvaddstr(y0 + 1 + visible, x0, "└");
        for (int c = 0; c < w; c++)
            addstr("─");
        addstr("┘");
        attroff(ft_attr(FT_DIM));
        cm_put(y0 + 2 + visible, x0, " Retorn recupera la versió · Esc tanca", COLS - 2, ft_attr(FT_DIM));
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
        if (cm_confirm(q, NULL, NULL)) {
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
static void diary_note(View *v)
{
    (void)diary_record(v->p->root, (long)project_words(v->p), time(NULL));
}

static void open_file_line(View *v, FNode *file, size_t line, size_t scene)
{
    char *path = strdup(file->path), err[200];
    int rc;

    if (!path)
        return;
    auto_snapshot(v, file);
    v->msg[0] = '\0';
    rc = launch_editor(path, line, err, sizeof err);
    if (project_refresh(v->p, v->msg, sizeof v->msg) == 0) {
        FNode *keep = NULL;

        diary_note(v);

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

static void open_in_editor(View *v, FNode *n)
{
    FNode *file = n->kind == FK_SCENE ? n->parent : n;
    size_t scene = 0;

    if (n->kind == FK_SCENE)
        for (size_t i = 0; i < file->nkids; i++)
            if (file->kids[i] == n)
                scene = i + 1;
    open_file_line(v, file, n->kind == FK_SCENE ? n->line + 1 : 0, scene);
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
    if (!cm_prompt(label, "", title, sizeof title, NULL, NULL))
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
    if (!cm_prompt("Títol nou: ", n->title, title, sizeof title, NULL, NULL))
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
    if (!cm_confirm(q, NULL, NULL))
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

/* ---------- enllaços [[ ]] ---------- */

/* Llista desplaçable a pantalla; retorna l'índex triat o -1. hook (opcional)
 * rep cada tecla abans de la resta: 0 = no la gestiona, 1 = gestionada (es
 * repinta), 2 = cal tornar a construir la llista (pick_list_ex retorna -2).
 * *sel és la selecció inicial i final. */
typedef int (*PickHook)(void *ctx, int key, long sel);

static int pick_list_ex(View *v, const char *title, char **lines, size_t n, const char *footer,
                        PickHook hook, void *ctx, long *selp)
{
    long sel = selp ? *selp : 0;
    int w = COLS - 4 > 100 ? 100 : COLS - 4;

    if (w < 20)
        w = 20;
    for (;;) {
        int x0 = 1, y0 = 2, key, visible = LINES - 7 < (int)n ? LINES - 7 : (int)n;
        long top;

        if (visible < 1)
            visible = 1;
        top = sel >= visible ? sel - visible + 1 : 0;
        paint(v);
        attrset(A_NORMAL);
        attron(ft_attr(FT_DIM));
        mvaddstr(y0, x0, "┌");
        for (int c = 0; c < w; c++)
            addstr("─");
        addstr("┐");
        attroff(ft_attr(FT_DIM));
        cm_put(y0, x0 + 1, title, w, ft_attr(FT_HEAD));
        for (int r = 0; r < visible; r++) {
            attr_t a = top + r == sel ? ft_attr(FT_SEL) : 0;

            attron(ft_attr(FT_DIM));
            mvaddstr(y0 + 1 + r, x0, "│");
            mvaddstr(y0 + 1 + r, x0 + w + 1, "│");
            attroff(ft_attr(FT_DIM));
            attron(a);
            move(y0 + 1 + r, x0 + 1);
            for (int c = 0; c < w; c++)
                addch(' ');
            attroff(a);
            cm_put(y0 + 1 + r, x0 + 2, lines[top + r], w - 2, a);
        }
        attron(ft_attr(FT_DIM));
        mvaddstr(y0 + 1 + visible, x0, "└");
        for (int c = 0; c < w; c++)
            addstr("─");
        addstr("┘");
        attroff(ft_attr(FT_DIM));
        cm_put(y0 + 2 + visible, x0, footer, COLS - 2, ft_attr(FT_DIM));
        refresh();
        key = key_read();
        if (key < 0 || key == K_FN(KEY_RESIZE))
            continue;
        if (hook) {
            int h = hook(ctx, key, sel);

            if (selp)
                *selp = sel;
            if (h == 2)
                return -2;
            if (h == 1)
                continue;
        }
        if (key == 27 || key == 'q') {
            if (selp)
                *selp = sel;
            return -1;
        }
        if (key == K_FN(KEY_UP) && sel > 0)
            sel--;
        else if (key == K_FN(KEY_DOWN) && sel + 1 < (long)n)
            sel++;
        else if (key == K_FN(KEY_HOME))
            sel = 0;
        else if (key == K_FN(KEY_END))
            sel = (long)n - 1;
        else if (key == K_FN(KEY_PPAGE))
            sel = sel > 8 ? sel - 8 : 0;
        else if (key == K_FN(KEY_NPAGE))
            sel = sel + 8 < (long)n ? sel + 8 : (long)n - 1;
        else if (key == '\n') {
            if (selp)
                *selp = sel;
            return (int)sel;
        }
    }
}

static int pick_list(View *v, const char *title, char **lines, size_t n, const char *footer)
{
    return pick_list_ex(v, title, lines, n, footer, NULL, NULL, NULL);
}

/* «Capítol › Escena» o «Fitxa: títol» d'un enllaç */
static void link_origin(const FBack *b, char *out, size_t sz)
{
    const FNode *f = b->file;

    if (f->kind == FK_CHAPTER && b->link->scene && b->link->scene <= f->nkids)
        snprintf(out, sz, "%s › %s", f->title, f->kids[b->link->scene - 1]->title);
    else if (f->kind == FK_CHAPTER)
        snprintf(out, sz, "%s", f->title);
    else
        snprintf(out, sz, "%s: %s", f->parent ? f->parent->title : "Fitxa", f->title);
}

static void free_lines(char **l, size_t n)
{
    for (size_t i = 0; i < n; i++)
        free(l[i]);
    free(l);
}

static char **back_lines(const FBack *b, size_t n, int show_target)
{
    char **lines = malloc((n ? n : 1) * sizeof *lines);

    for (size_t i = 0; i < n; i++) {
        char origin[240], buf[500];

        link_origin(&b[i], origin, sizeof origin);
        if (show_target)
            snprintf(buf, sizeof buf, "[[%s]]  ·  %s  ·  %s", b[i].link->target, origin, b[i].link->context);
        else
            snprintf(buf, sizeof buf, "%s  ·  %s", origin, b[i].link->context);
        lines[i] = strdup(buf);
    }
    return lines;
}

/* u: on surt el capítol o la fitxa seleccionats (els [[ ]] d'altres fitxers) */
static void act_where(View *v)
{
    FNode *n = cur(v);
    FBack *b;
    size_t cnt;
    char title[200], **lines;
    int pick;

    if (n && n->kind == FK_SCENE)
        n = n->parent;
    if (!n || (n->kind != FK_CHAPTER && n->kind != FK_CARD)) {
        snprintf(v->msg, sizeof v->msg, "Els enllaços apunten a capítols i fitxes.");
        return;
    }
    cnt = project_backlinks(v->p, n, &b);
    if (!cnt) {
        snprintf(v->msg, sizeof v->msg, "Cap [[enllaç]] apunta a «%.60s».", n->title);
        return;
    }
    lines = back_lines(b, cnt, 0);
    snprintf(title, sizeof title, " On surt «%s» (%zu) ", n->title, cnt);
    pick = pick_list(v, title, lines, cnt, " Retorn obre el text a Phileas, a la línia · Esc tanca");
    if (pick >= 0)
        open_file_line(v, b[pick].file, b[pick].link->line + 1, b[pick].link->scene);
    free_lines(lines, cnt);
    free(b);
}

/* t: els enllaços sense destí de tot el projecte */
static void act_broken(View *v)
{
    FBack *b;
    size_t cnt = project_broken_links(v->p, &b);
    char title[120], **lines;
    int pick;

    if (!cnt) {
        snprintf(v->msg, sizeof v->msg, "Cap enllaç trencat.");
        return;
    }
    lines = back_lines(b, cnt, 1);
    snprintf(title, sizeof title, " Enllaços sense fitxa (%zu) ", cnt);
    pick = pick_list(v, title, lines, cnt, " Retorn obre el text a Phileas, a la línia · Esc tanca");
    if (pick >= 0)
        open_file_line(v, b[pick].file, b[pick].link->line + 1, b[pick].link->scene);
    free_lines(lines, cnt);
    free(b);
}

/* l: els enllaços de l'escena, el capítol o la fitxa seleccionats */
static void act_outgoing(View *v)
{
    FNode *n = cur(v), *file;
    size_t scene = 0, cnt = 0;
    char **lines, title[200];
    const FLink **ls;
    int pick;

    if (!n || n->kind == FK_GROUP) {
        snprintf(v->msg, sizeof v->msg, "Selecciona una escena, un capítol o una fitxa.");
        return;
    }
    file = n->kind == FK_SCENE ? n->parent : n;
    if (n->kind == FK_SCENE)
        for (size_t i = 0; i < file->nkids; i++)
            if (file->kids[i] == n)
                scene = i + 1;
    ls = malloc((file->nlinks ? file->nlinks : 1) * sizeof *ls);
    for (size_t i = 0; i < file->nlinks; i++)
        if (!scene || file->links[i].scene == scene)
            ls[cnt++] = &file->links[i];
    if (!cnt) {
        free(ls);
        snprintf(v->msg, sizeof v->msg, "«%.60s» no té cap [[enllaç]].", n->title);
        return;
    }
    lines = malloc(cnt * sizeof *lines);
    for (size_t i = 0; i < cnt; i++) {
        char buf[400];

        if (ls[i]->to)
            snprintf(buf, sizeof buf, "[[%s]]  →  %s (%s)", ls[i]->target, ls[i]->to->title,
                     ls[i]->to->kind == FK_CHAPTER ? "capítol" : ls[i]->to->parent ? ls[i]->to->parent->title : "fitxa");
        else
            snprintf(buf, sizeof buf, "[[%s]]  →  (sense fitxa)  ·  %s", ls[i]->target, ls[i]->context);
        lines[i] = strdup(buf);
    }
    snprintf(title, sizeof title, " Enllaços de «%s» (%zu) ", n->title, cnt);
    pick = pick_list(v, title, lines, cnt, " Retorn va a la fitxa (o obre el text si no en té) · Esc tanca");
    if (pick >= 0) {
        if (ls[pick]->to) {
            FNode *to = ls[pick]->to;
            FNode *g = group_of(to);

            v->mode = 0;
            if (g)
                g->open = 1;
            v->filter[0] = '\0';
            rebuild(v, to);
            snprintf(v->msg, sizeof v->msg, "«%.80s»: Retorn l'obre", to->title);
        } else {
            open_file_line(v, file, ls[pick]->line + 1, ls[pick]->scene);
        }
    }
    free_lines(lines, cnt);
    free(ls);
}

/* ---------- cerca i substitució al projecte ---------- */

typedef struct {
    unsigned       *opts;
    unsigned char  *sel;
    char          **lines;
    size_t          n;
    int             replace;
} FindCtx;

static int find_hook(void *c, int key, long sel)
{
    FindCtx *x = c;

    switch (key) {
    case 'c': *x->opts ^= FF_ICASE; return 2;
    case 'w': *x->opts ^= FF_WORD; return 2;
    case 'm': *x->opts ^= FF_META; return 2;
    case ' ':
        if (x->replace && x->n && sel >= 0 && sel < (long)x->n) {
            x->sel[sel] = !x->sel[sel];
            x->lines[sel][1] = x->sel[sel] ? 'x' : ' ';
        }
        return 1;
    case 'a':
        if (x->replace && x->n) {
            int any = 0;

            for (size_t i = 0; i < x->n; i++)
                any |= x->sel[i];
            for (size_t i = 0; i < x->n; i++) {
                x->sel[i] = !any;
                x->lines[i][1] = x->sel[i] ? 'x' : ' ';
            }
        }
        return 1;
    default:
        return 0;
    }
}

/* f: cerca; R: substitueix. */
static void act_find(View *v, int replace)
{
    char needle[200], repl[200] = "", err[240];
    unsigned opts = replace ? FF_WORD : FF_ICASE;

    if (!cm_prompt(replace ? "Substitueix: " : "Cerca al projecte: ", "", needle, sizeof needle, NULL, NULL) || !needle[0])
        return;
    if (replace && !cm_prompt("per (buit = esborrar): ", "", repl, sizeof repl, NULL, NULL))
        return;
    for (;;) {
        FFind f;
        FindCtx x;
        char **lines, title[400], footer[400];
        unsigned char *sel;
        size_t n;
        int pick;
        long selpos = 0;

        if (find_in_project(v->p, needle, opts, &f, err, sizeof err) != 0) {
            snprintf(v->msg, sizeof v->msg, "%.190s", err);
            return;
        }
        n = f.n;
        lines = malloc((n ? n : 1) * sizeof *lines);
        sel = calloc(n ? n : 1, 1);
        for (size_t i = 0; i < n; i++) {
            FLink fake;
            FBack fb;
            char origin[240], buf[600];

            memset(&fake, 0, sizeof fake);
            fake.scene = f.hits[i].scene;
            fb.file = f.hits[i].file;
            fb.link = &fake;
            link_origin(&fb, origin, sizeof origin);
            sel[i] = 1;
            snprintf(buf, sizeof buf, "%s%s  ·  %s", replace ? "[x] " : "", origin, f.hits[i].context);
            lines[i] = strdup(buf);
        }
        if (!n) {
            lines[0] = strdup("(cap coincidència amb aquestes opcions)");
            n = 1;
        }
        {
            char fl[96];

            snprintf(fl, sizeof fl, "c majúscules: %s · w paraules senceres: %s · m metadades: %s",
                     (opts & FF_ICASE) ? "ignora" : "compten", (opts & FF_WORD) ? "sí" : "no",
                     (opts & FF_META) ? "inclou" : "no");
            if (replace) {
                snprintf(title, sizeof title, " Substitueix «%.60s» per «%.60s»: %zu coincidències en %zu fitxers ",
                         needle, repl, f.n, f.files);
                snprintf(footer, sizeof footer, " Retorn aplica els marcats · Espai marca · a tots/cap · %s", fl);
            } else {
                snprintf(title, sizeof title, " Cerca «%.100s»: %zu coincidències en %zu fitxers ", needle, f.n, f.files);
                snprintf(footer, sizeof footer, " Retorn obre a Phileas · %s", fl);
            }
        }
        x.opts = &opts;
        x.sel = sel;
        x.lines = lines;
        x.n = f.n;
        x.replace = replace;
        pick = pick_list_ex(v, title, lines, n, footer, find_hook, &x, &selpos);
        if (pick == -2) {
            free_lines(lines, n);
            free(sel);
            find_free(&f);
            continue;
        }
        if (pick >= 0 && f.n) {
            if (!replace) {
                open_file_line(v, f.hits[pick].file, f.hits[pick].line + 1, f.hits[pick].scene);
            } else {
                size_t marked = 0, files = 0, count = 0;
                FNode *last = NULL;

                for (size_t i = 0; i < f.n; i++)
                    if (sel[i]) {
                        marked++;
                        if (f.hits[i].file != last) {
                            files++;
                            last = f.hits[i].file;
                        }
                    }
                if (!marked) {
                    snprintf(v->msg, sizeof v->msg, "No hi ha cap coincidència marcada.");
                } else {
                    char q[300];

                    snprintf(q, sizeof q, "Substituir %zu coincidències en %zu fitxers? Se'n fa una instantània abans.",
                             marked, files);
                    if (cm_confirm(q, NULL, NULL)) {
                        if (find_replace(v->p, &f, sel, repl, time(NULL), &files, &count, err, sizeof err) != 0) {
                            snprintf(v->msg, sizeof v->msg, "%.190s", err);
                            refresh_select(v, NULL);
                            snprintf(v->msg, sizeof v->msg, "%.190s", err);
                        } else {
                            refresh_select(v, NULL);
                            snprintf(v->msg, sizeof v->msg, "%zu substitucions en %zu fitxers (instantànies: v)", count, files);
                        }
                    }
                }
            }
        }
        free_lines(lines, n);
        free(sel);
        find_free(&f);
        return;
    }
}

/* C: compila el manuscrit a un fitxer (.md, o .docx/.epub/.pdf amb pandoc) */
static void act_compile(View *v)
{
    char name[300], path[700], err[240], *slug = fk_slug(v->p->titol), initial[200];
    CompileOpts o;
    CompileStats st;

    snprintf(initial, sizeof initial, "%s.md", slug);
    free(slug);
    if (!cm_prompt("Compila a (.md, .docx, .epub, .pdf…): ", initial, name, sizeof name, NULL, NULL) || !name[0])
        return;
    if (name[0] == '/' || name[0] == '.' || strchr(name, '/'))
        snprintf(path, sizeof path, "%s", name);
    else
        snprintf(path, sizeof path, "%s/%s", v->p->root, name);
    memset(&o, 0, sizeof o);
    if (project_compile_file(v->p, &o, path, &st, err, sizeof err) != 0) {
        snprintf(v->msg, sizeof v->msg, "%.190s", err);
        return;
    }
    if (st.pending)
        snprintf(v->msg, sizeof v->msg, "Compilat: %zu capítols, %zu paraules → %.60s (%zu pendents TODO/[?])",
                 st.chapters, st.words, name, st.pending);
    else
        snprintf(v->msg, sizeof v->msg, "Compilat: %zu capítols, %zu paraules → %.80s", st.chapters, st.words, name);
}

/* Menú de l'Esc: la mateixa acció que cada tecla. Retorna el codi de la tecla
 * de l'acció triada, o -1 si es tanca sense triar. */
/* T: la línia temporal: les escenes per data, amb els salts enrere i les sense data */
static void act_timeline(View *v)
{
    Timeline t;
    char **lines, title[160], foot[200];
    long sel = 0;

    if (timeline_build(v->p, &t) != 0 || !t.n) {
        timeline_free(&t);
        snprintf(v->msg, sizeof v->msg, "El manuscrit no té escenes ni capítols.");
        return;
    }
    lines = calloc(t.n, sizeof *lines);
    if (!lines) {
        timeline_free(&t);
        return;
    }
    for (size_t i = 0; i < t.n; i++) {
        const TLItem *it = &t.items[i];
        const FNode *n = it->node;
        char buf[400], where[250];

        if (n->kind == FK_SCENE)
            snprintf(where, sizeof where, "%s · %s", n->parent->title, n->title);
        else
            snprintf(where, sizeof where, "%s", n->title);
        snprintf(buf, sizeof buf, "%-10s  %s%s%s%s%s", it->dated ? n->data : it->bad ? "data ?" : "sense data",
                 where, n->pov && *n->pov ? " · " : "", n->pov ? n->pov : "",
                 it->back ? "   ↶ salt enrere" : "", it->bad ? "   (data no vàlida)" : "");
        lines[i] = strdup(buf);
        if (!lines[i])
            lines[i] = strdup("");
    }
    snprintf(title, sizeof title, " Línia temporal · %zu amb data, %zu sense · %zu salts enrere ", t.dated,
             t.undated, t.backs);
    snprintf(foot, sizeof foot, " Retorn obre a Phileas · ↶ = el text hi arriba després d'una escena posterior · Esc tanca");
    for (;;) {
        int pick = pick_list_ex(v, title, lines, t.n, foot, NULL, NULL, &sel);

        if (pick < 0)
            break;
        open_in_editor(v, t.items[pick].node);
        break;
    }
    free_lines(lines, t.n);
    timeline_free(&t);
}

static void act_diary(View *v)
{
    Diary d;
    char **l;
    size_t n;

    diary_note(v);
    if (diary_load(v->p->root, &d) != 0) {
        snprintf(v->msg, sizeof v->msg, "No es pot llegir el diari.");
        return;
    }
    l = diary_report(&d, (long)project_words(v->p), v->p->objectiu, v->p->data_limit, time(NULL), &n);
    diary_free(&d);
    if (!l)
        return;
    (void)pick_list(v, "Diari d'escriptura", l, n, " Esc/q: tanca ");
    free_lines(l, n);
}

/* Menú d'Esc, agrupat per categories. Una entrada amb key 0 és una capçalera
 * (no es pot triar). */
/* b: obre Recerca amb el vault del camp «recerca:» de projecte.md (relatiu
 * a la carpeta del projecte) o, si no n'hi ha, el vault per defecte de
 * Recerca (recerca-vault del phileasrc). En sortir-ne, es torna aquí. */
static void act_research(View *v)
{
    char path[PATH_MAX + 32], vault[PATH_MAX], full[2 * PATH_MAX], err[180];
    char *argv[3] = { "recerca", NULL, NULL };
    FILE *f;

    vault[0] = '\0';
    snprintf(path, sizeof path, "%s/projecte.md", v->p->root);
    if ((f = fopen(path, "rb"))) {
        char buf[16384];
        size_t n = fread(buf, 1, sizeof buf - 1, f);

        buf[n] = '\0';
        fclose(f);
        yaml_get(buf, n, "recerca", vault, sizeof vault);
    }
    if (vault[0]) {
        if (vault[0] == '~' && vault[1] == '/' && getenv("HOME"))
            snprintf(full, sizeof full, "%s/%s", getenv("HOME"), vault + 2);
        else if (vault[0] != '/')
            snprintf(full, sizeof full, "%s/%s", v->p->root, vault);
        else
            snprintf(full, sizeof full, "%s", vault);
        argv[1] = full;
    }
    if (launch_program(argv, err, sizeof err))
        snprintf(v->msg, sizeof v->msg, "Recerca: %s", err);
}

static int menu_run(View *v)
{
    static const CmMenuItem items[] = {
        { "Fitxers", NULL, 0 },
        { "Obre a Phileas", "Retorn", '\n' },
        { "Nou capítol o fitxa", "n", 'n' },
        { "Canvia el títol", "r", 'r' },
        { "Canvia l'estat", "e", 'e' },
        { "Puja el capítol", "Alt+↑", '<' },
        { "Baixa el capítol", "Alt+↓", '>' },
        { "Mou a la paperera", "Supr", K_FN(KEY_DC) },
        { "Vista", NULL, 0 },
        { "Canvia de vista (arbre/taula)", "Tab", 9 },
        { "Desplega o plega", "Espai", ' ' },
        { "Filtra", "/", '/' },
        { "Enllaços i cerca", NULL, 0 },
        { "On surt (on s'usa)", "u", 'u' },
        { "Enllaços d'aquí", "l", 'l' },
        { "Enllaços trencats", "t", 't' },
        { "Cerca al projecte", "f", 'f' },
        { "Recerca (fitxes de recerca)", "b", 'b' },
        { "Substitueix al projecte", "R", 'R' },
        { "Manuscrit", NULL, 0 },
        { "Línia temporal", "T", 'T' },
        { "Diari i estadístiques", "d", 'd' },
        { "Compila el manuscrit", "C", 'C' },
        { "Versions", NULL, 0 },
        { "Desa una instantània", "s", 's' },
        { "Versions anteriors", "v", 'v' },
        { "Fàbula", NULL, 0 },
        { "Ajuda", "F1", K_FN(KEY_F(1)) },
        { "Surt", "Ctrl+Q", 17 },
    };

    return cm_menu(items, (int)(sizeof items / sizeof *items), paint_cb, v);
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
        "  u / l / t                  on surt un capítol o fitxa / els seus [[enllaços]] / enllaços trencats",
        "  f / R                      cerca / substitueix al projecte (c, w, m: opcions)",
        "  b                          obre Recerca (el vault de «recerca:» a projecte.md); en sortir, torna",
        "  C                          compila el manuscrit en un sol fitxer (.md, .docx, .epub…)",
        "  T                          línia temporal: escenes per data, amb els salts enrere",
        "  d                          diari d'escriptura: avui, setmana, ratxa i previsió",
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

    cm_help(lines, sizeof lines / sizeof *lines);
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
    if (!cm_prompt(label, cell, value, sizeof value, NULL, NULL))
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
    diary_note(&v);
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
        case 'T':
            act_timeline(&v);
            break;
        case 'd':
            act_diary(&v);
            break;
        case 'f':
            act_find(&v, 0);
            break;
        case 'C':
            act_compile(&v);
            break;
        case 'R':
            act_find(&v, 1);
            break;
        case 'u':
            act_where(&v);
            break;
        case 'l':
            act_outgoing(&v);
            break;
        case 't':
            act_broken(&v);
            break;
        case 'b':
            act_research(&v);
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
