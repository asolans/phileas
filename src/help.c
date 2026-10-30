#include "help.h"
#include "command.h"
#include "config.h"
#include "keys.h"
#include "render.h"
#include "tui.h"
#include "utf8.h"
#include "util.h"
#include "version.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wctype.h>

/* Seccions de l'ajuda, amb les ordres en l'ordre en què es mostren. Les
 * ordres que no hi són van a «Altres», al final. */
static const struct {
    const char *title;
    const char *const *names;
} sections[] = {
    { "Fitxer", (const char *const[]){ "obrir", "recents", "nou", "desar", "desar-com", "sortir", NULL } },
    { "Moviment", (const char *const[]){
        "moure-amunt", "moure-avall", "moure-esquerra", "moure-dreta", "inici-linia",
        "fi-linia", "pagina-amunt", "pagina-avall", "paraula-anterior", "paraula-seguent",
        "paragraf-anterior", "paragraf-seguent", "inici-document", "fi-document",
        "anar-a-linia", "esquema", NULL } },
    { "Selecció i porta-retalls", (const char *const[]){
        "marca", "seleccionar-tot", "cancellar", "tallar", "copiar", "enganxar",
        "seleccio-esquerra", "seleccio-dreta", "seleccio-amunt", "seleccio-avall",
        "seleccio-inici-linia", "seleccio-fi-linia", "seleccio-paraula-anterior",
        "seleccio-paraula-seguent", "seleccio-paragraf-anterior", "seleccio-paragraf-seguent",
        "seleccio-pagina-amunt", "seleccio-pagina-avall", "seleccio-inici-document",
        "seleccio-fi-document", NULL } },
    { "Edició", (const char *const[]){
        "nova-linia", "tabulador", "esborrar-enrere", "esborrar-endavant",
        "esborrar-paraula-anterior", "esborrar-paraula-seguent", "pujar-paragraf",
        "baixar-paragraf", "tipografia", "desfer", "refer", NULL } },
    { "Cerca", (const char *const[]){
        "cercar", "cercar-seguent", "cercar-anterior", "substituir", NULL } },
    { "Ortografia", (const char *const[]){
        "ortografia", "alternar-idioma", "revisar-ortografia", "suggeriments", "idioma", NULL } },
    { "Vista", (const char *const[]){
        "maquina-escriure", "centrar", "colors", "enfocament", "tema", "redibuixar", NULL } },
    { "Configuració i ajuda", (const char *const[]){
        "menu", "ajuda", "paleta", "recarregar-configuracio", "desar-configuracio", "autodesat",
        "quant-a", NULL } },
};
#define NSECTIONS (sizeof sections / sizeof sections[0])

/* Text fix del principi */
static const char *const intro[] = {
    "Esc (menú): les ordres per temes; la lletra o Enter tria, ← o Esc torna enrere.",
    "Ctrl+P (paleta): executa qualsevol ordre pel nom; Tab completa el nom.",
    "Les tecles es canvien al phileasrc: tecla.NOM = ordre.",
    "Navegador (Ctrl+O): Enter obre, ← puja, escriu per filtrar.",
    "Fitxer nou sense extensió (navegador, Desa com): es crea amb .md.",
    "Recents (Alt+O): Enter obre, Supr treu de la llista; el cursor torna on era.",
    "Esquema (F2): capçaleres i paraules de cada secció; Enter hi va, Alt+↑/↓ mou la secció.",
    "Suggeriments (Alt+Enter): el número substitueix, a afegeix, i ignora.",
    "Porta-retalls: a l'escriptori, Ctrl+C/Ctrl+V el comparteixen amb les altres aplicacions.",
};
#define NINTRO (sizeof intro / sizeof intro[0])

typedef struct {
    int  kind;          /* 0 = text, 1 = títol de secció, 2 = ordre, 3 = ordre de la paleta */
    char keys[96];
    const char *name;   /* ordre */
    const char *text;   /* descripció o text */
} Row;

static Row  *rows;
static size_t nrows, cap;

static Row *add_row(int kind)
{
    if (nrows == cap) {
        cap = cap ? cap * 2 : 128;
        rows = xrealloc(rows, cap * sizeof *rows);
    }
    memset(&rows[nrows], 0, sizeof *rows);
    rows[nrows].kind = kind;
    return &rows[nrows++];
}

static void add_command(const Command *c)
{
    Row *r = add_row(2);
    int keys[8], n = cmd_keys(c, keys, 8);
    size_t len = 0;

    for (int i = 0; i < n; i++) {
        char k[32];

        key_describe(keys[i], k, sizeof k);
        if (len + strlen(k) + 3 >= sizeof r->keys)
            break;
        len += (size_t)snprintf(r->keys + len, sizeof r->keys - len, "%s%s", i ? ", " : "", k);
    }
    r->name = c->name;
    r->text = c->desc;
}

/* On és la configuració de l'usuari i com es crea (el camí depèn de
 * PHILEAS_CONFIG: a la BYOK, /data/phileas) */
static void add_config_rows(void)
{
    static char where[1200];
    char path[1100];

    config_user_path(path, sizeof path);
    snprintf(where, sizeof where, "Configuració pròpia: %s (mana sobre la del sistema).", path);
    add_row(0)->text = where;
    add_row(0)->text = "Per crear-la amb totes les opcions i els valors actuals: Ctrl+P";
    add_row(0)->text = "  desar-configuracio (l'anterior queda a phileasrc.anterior).";
}

static void build(void)
{
    size_t ncmd = cmd_count();
    char *shown = xmalloc(ncmd);
    int others = 0;

    nrows = 0;
    memset(shown, 0, ncmd);
    for (size_t i = 0; i < NINTRO; i++)
        add_row(0)->text = intro[i];
    add_config_rows();

    /* Les ordres sense tecla, que només es poden fer servir amb la paleta */
    add_row(1)->text = "Ordres de la paleta (Ctrl+P i el nom; no tenen tecla)";
    for (size_t i = 0; i < ncmd; i++) {
        int k;

        if (cmd_keys(cmd_at(i), &k, 1) == 0) {
            Row *r = add_row(3);

            r->name = cmd_at(i)->name;
            r->text = cmd_at(i)->desc;
        }
    }

    for (size_t s = 0; s < NSECTIONS; s++) {
        add_row(1)->text = sections[s].title;
        for (const char *const *n = sections[s].names; *n; n++) {
            const Command *c = cmd_find(*n);

            if (c) {
                add_command(c);
                shown[c - cmd_at(0)] = 1;
            }
        }
    }
    for (size_t i = 0; i < ncmd; i++) {
        if (shown[i])
            continue;
        if (!others++)
            add_row(1)->text = "Altres";
        add_command(cmd_at(i));
    }
    free(shown);
}

/* Coincidència sense distingir majúscules (també amb accents) */
static int contains(const char *s, const char *pat)
{
    size_t sl = strlen(s), pl = strlen(pat);

    for (size_t i = 0; i <= sl; i = utf8_next(s, sl, i)) {
        size_t a = i, b = 0;

        for (;;) {
            wchar_t x, y;

            if (b >= pl)
                return 1;
            if (a >= sl)
                break;
            a += utf8_decode(s, sl, a, &x);
            b += utf8_decode(pat, pl, b, &y);
            if (towlower((wint_t)x) != towlower((wint_t)y))
                break;
        }
        if (i == sl)
            break;
    }
    return 0;
}

static int row_matches(const Row *r, const char *filter)
{
    if (!*filter)
        return 1;
    if (r->kind < 2)
        return 0;
    return contains(r->keys, filter) || contains(r->name, filter) || contains(r->text, filter);
}

static int put(const char *s, int maxw)
{
    size_t len = strlen(s), i = 0;
    int col = 0;

    while (i < len) {
        wchar_t wc;
        size_t n = utf8_decode(s, len, i, &wc);
        int w;

        if (!utf8_printable(wc))
            wc = L'?';
        w = utf8_width(wc, col);
        if (col + w > maxw)
            break;
        addnwstr(&wc, 1);
        col += w;
        i += n;
    }
    return col;
}

static void draw(size_t *vis, size_t nvis, size_t top, const char *filter)
{
    int h = LINES - 3, keyw = COLS >= 60 ? 22 : COLS / 3;

    erase();
    attrset(ui_attr(UI_BAR));
    mvhline(0, 0, ' ', COLS);
    move(0, 1);
    put("Ajuda de Phileas: dreceres i ordres", COLS - 2);
    attrset(A_NORMAL);

    for (int y = 0; y < h && top + (size_t)y < nvis; y++) {
        const Row *r = &rows[vis[top + (size_t)y]];
        int col;

        move(y + 1, 0);
        if (r->kind == 0) {
            move(y + 1, 1);
            put(r->text, COLS - 2);
        } else if (r->kind == 1) {
            attron(A_BOLD);
            move(y + 1, 1);
            put(r->text, COLS - 2);
            attroff(A_BOLD);
        } else if (r->kind == 3) {
            /* El nom és el que s'escriu a la paleta: a la primera columna */
            int namew = (int)utf8_count(r->name, strlen(r->name));
            int w = namew + 2 > keyw ? namew + 2 : keyw;

            move(y + 1, 3);
            put(r->name, COLS - 4);
            if (3 + w < COLS - 1) {
                move(y + 1, 3 + w);
                put(r->text, COLS - 4 - w);
            }
        } else {
            move(y + 1, 3);
            if (r->keys[0])
                put(r->keys, keyw - 1);
            else {
                attron(A_DIM);
                put("paleta", keyw - 1);
                attroff(A_DIM);
            }
            move(y + 1, 3 + keyw);
            col = put(r->text, COLS - 4 - keyw);
            /* El nom de l'ordre (per a la paleta i el phileasrc), si hi cap sencer */
            if (col + 2 + (int)utf8_count(r->name, strlen(r->name)) <= COLS - 4 - keyw) {
                attron(A_DIM);
                addstr("  ");
                put(r->name, COLS);
                attroff(A_DIM);
            }
        }
    }

    attrset(ui_attr(UI_BAR));
    mvhline(LINES - 2, 0, ' ', COLS);
    move(LINES - 2, 1);
    put("↑ ↓ RePàg AvPàg Inici Fi · escriu per filtrar · Esc o F1 tanca", COLS - 2);
    attrset(A_NORMAL);
    move(LINES - 1, 0);
    attron(A_BOLD);
    put("Filtre: ", COLS);
    attroff(A_BOLD);
    put(filter, COLS - 8);
}

void help_show(Editor *e)
{
    char filter[64] = "";
    size_t *vis, nvis = 0, top = 0, len;
    int done = 0;

    (void)e;
    build();
    vis = xmalloc((nrows ? nrows : 1) * sizeof *vis);
    key_set_timeout(-1);
    while (!done) {
        int h = LINES > 4 ? LINES - 3 : 1, key;
        size_t maxtop;

        nvis = 0;
        for (size_t i = 0; i < nrows; i++)
            if (row_matches(&rows[i], filter))
                vis[nvis++] = i;
        maxtop = nvis > (size_t)h ? nvis - (size_t)h : 0;
        if (top > maxtop)
            top = maxtop;
        draw(vis, nvis, top, filter);
        refresh();

        key = key_read();
        if (key < 0)
            continue;
        switch (key) {
        case KEY_FLAG_FN | KEY_UP:
            if (top > 0)
                top--;
            break;
        case KEY_FLAG_FN | KEY_DOWN:
            top++;
            break;
        case KEY_FLAG_FN | KEY_PPAGE:
            top = top > (size_t)h ? top - (size_t)h : 0;
            break;
        case KEY_FLAG_FN | KEY_NPAGE:
            top += (size_t)h;
            break;
        case KEY_FLAG_FN | KEY_HOME:
            top = 0;
            break;
        case KEY_FLAG_FN | KEY_END:
            top = maxtop;
            break;
        case KEY_FLAG_FN | KEY_F(1):
            done = 1;
            break;
        case 27:
            if (filter[0])
                filter[0] = '\0';
            else
                done = 1;
            break;
        case KEY_FLAG_FN | KEY_BACKSPACE:
        case 8:
            if ((len = strlen(filter)) > 0)
                filter[utf8_prev(filter, len)] = '\0';
            top = 0;
            break;
        default:
            if (!(key & (KEY_FLAG_FN | KEY_FLAG_ALT)) && utf8_printable((wchar_t)key)) {
                char s[4];
                size_t n = utf8_encode((wchar_t)key, s);

                if ((len = strlen(filter)) + n < sizeof filter) {
                    memcpy(filter + len, s, n);
                    filter[len + n] = '\0';
                    top = 0;
                }
            }
        }
    }
    free(vis);
}

void about_show(Editor *e)
{
    char sys[128] = "";
    const char *lines[8];
    int n = 0, w = 0, h, x, y;
    FILE *f = fopen("/etc/byok-version", "r");

    if (f) {
        if (fgets(sys, sizeof sys, f))
            sys[strcspn(sys, "\n")] = '\0';
        fclose(f);
    }
    lines[n++] = "Phileas " PHILEAS_VERSION;
    lines[n++] = "Editor de text amb Markdown";
    if (sys[0])
        lines[n++] = sys;
    lines[n++] = "";
    lines[n++] = "Llicència GPL-3.0-or-later";
    lines[n++] = "github.com/asolans/phileas";
    for (int i = 0; i < n; i++) {
        int lw = (int)utf8_count(lines[i], strlen(lines[i]));
        if (lw > w)
            w = lw;
    }
    w += 6;
    h = n + 4;
    if (w > COLS)
        w = COLS;
    if (h > LINES)
        h = LINES;
    x = (COLS - w) / 2;
    y = (LINES - h) / 2;

    render(e);   /* sense els menús d'on s'ha cridat */
    attrset(ui_attr(UI_MENU));
    mvaddch(y, x, ACS_ULCORNER);
    hline(ACS_HLINE, w - 2);
    mvaddch(y, x + w - 1, ACS_URCORNER);
    for (int i = 1; i < h - 1; i++) {
        mvaddch(y + i, x, ACS_VLINE);
        mvhline(y + i, x + 1, ' ', w - 2);
        mvaddch(y + i, x + w - 1, ACS_VLINE);
    }
    mvaddch(y + h - 1, x, ACS_LLCORNER);
    hline(ACS_HLINE, w - 2);
    mvaddch(y + h - 1, x + w - 1, ACS_LRCORNER);
    if (w > 14) {
        move(y, x + 2);
        attron(A_BOLD);
        put(" Quant a ", w - 4);
        attroff(A_BOLD);
    }
    for (int i = 0; i < n && 2 + i < h - 1; i++) {
        move(y + 2 + i, x + 3);
        if (i == 0)
            attron(A_BOLD);
        put(lines[i], w - 6);
        attroff(A_BOLD);
    }
    attrset(A_NORMAL);
    refresh();

    key_set_timeout(-1);
    while (key_read() < 0)
        ;
}
