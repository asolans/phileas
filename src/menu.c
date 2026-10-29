#include "menu.h"
#include "keys.h"
#include "render.h"
#include "tui.h"
#include "utf8.h"

#include <string.h>
#include <wctype.h>

typedef struct Menu Menu;

typedef struct {
    char        key;     /* lletra que la tria dins el menú; 0 = cap */
    const char *label;
    const char *cmd;     /* ordre; NULL si obre un submenú */
    const char *args;    /* arguments (ordres externes i «tema») */
    const Menu *sub;
} MenuItem;

struct Menu {
    const char     *title;
    const MenuItem *items;   /* acaba amb label NULL */
};

/* Les opcions amb una ordre que no existeix no es mostren, ni els submenús
 * que es queden buits (el de BYOK, sense ordres externes). */

static const Menu m_file = { "Fitxer", (const MenuItem[]){
    { 'o', "Obre…",                "obrir",     NULL, NULL },
    { 'n', "Nou",                  "nou",       NULL, NULL },
    { 'd', "Desa",                 "desar",     NULL, NULL },
    { 'c', "Desa com…",            "desar-com", NULL, NULL },
    { 's', "Surt",                 "sortir",    NULL, NULL },
    { 0 } } };

static const Menu m_edit = { "Edita", (const MenuItem[]){
    { 'd', "Desfés",               "desfer",          NULL, NULL },
    { 'r', "Refés",                "refer",           NULL, NULL },
    { 't', "Talla",                "tallar",          NULL, NULL },
    { 'c', "Copia",                "copiar",          NULL, NULL },
    { 'e', "Enganxa",              "enganxar",        NULL, NULL },
    { 's', "Selecciona-ho tot",    "seleccionar-tot", NULL, NULL },
    { 'm', "Comença una selecció", "marca",           NULL, NULL },
    { 0 } } };

static const Menu m_find = { "Cerca", (const MenuItem[]){
    { 'c', "Cerca…",               "cercar",          NULL, NULL },
    { 's', "Següent",              "cercar-seguent",  NULL, NULL },
    { 'a', "Anterior",             "cercar-anterior", NULL, NULL },
    { 'u', "Substitueix…",         "substituir",      NULL, NULL },
    { 'e', "Esquema…",             "esquema",         NULL, NULL },
    { 'l', "Ves a la línia…",      "anar-a-linia",    NULL, NULL },
    { 'i', "Inici del document",   "inici-document",  NULL, NULL },
    { 'f', "Final del document",   "fi-document",     NULL, NULL },
    { 0 } } };

static const Menu m_spell = { "Ortografia", (const MenuItem[]){
    { 'o', "Marca o no els errors", "ortografia",         NULL, NULL },
    { 'a', "Alterna l'idioma",      "alternar-idioma",    NULL, NULL },
    { 'e', "Error següent",         "revisar-ortografia", NULL, NULL },
    { 's', "Suggeriments",          "suggeriments",       NULL, NULL },
    { 'd', "Tria el diccionari…",   "idioma",             NULL, NULL },
    { 0 } } };

static const Menu m_theme = { "Tema", (const MenuItem[]){
    { 't', "Terminal",             "tema", "terminal", NULL },
    { 'f', "Fosc",                 "tema", "fosc",     NULL },
    { 'c', "Clar",                 "tema", "clar",     NULL },
    { 's', "Sèpia",                "tema", "sepia",    NULL },
    { 'o', "Fòsfor",               "tema", "fosfor",   NULL },
    { 'w', "WordPerfect",          "tema", "wordperfect", NULL },
    { 'e', "El següent",           "tema", NULL,       NULL },
    { 0 } } };

static const Menu m_view = { "Vista", (const MenuItem[]){
    { 'm', "Màquina d'escriure",   "maquina-escriure", NULL, NULL },
    { 'c', "Centra la columna",    "centrar",          NULL, NULL },
    { 'o', "Colors de Markdown",   "colors",           NULL, NULL },
    { 't', "Tema",                 NULL,               NULL, &m_theme },
    { 'r', "Redibuixa",            "redibuixar",       NULL, NULL },
    { 0 } } };

static const Menu m_help = { "Ajuda", (const MenuItem[]){
    { 'a', "Ajuda",                "ajuda",                   NULL, NULL },
    { 'p', "Paleta d'ordres…",     "paleta",                  NULL, NULL },
    { 'd', "Autodesat",            "autodesat",               NULL, NULL },
    { 'r', "Recarrega la configuració", "recarregar-configuracio", NULL, NULL },
    { 'c', "Desa la configuració",      "desar-configuracio",      NULL, NULL },
    { 'q', "Quant a Phileas",      "quant-a",                 NULL, NULL },
    { 0 } } };

/* Ordres externes de la imatge BYOK (phileasrc). Les altres externes que
 * hi hagi s'afegeixen al final amb el seu nom. */
static const MenuItem byok_known[] = {
    { 'g', "Lletra més gran",      "lletra",  "+",  NULL },
    { 'p', "Lletra més petita",    "lletra",  "-",  NULL },
    { 'r', "Gira la pantalla",     "gira",    NULL, NULL },
    { 'e', "Exporta al pendrive",  "exporta", NULL, NULL },
    { 'n', "Envia a un ordinador", "envia",   NULL, NULL },
};
#define NBYOK_KNOWN (sizeof byok_known / sizeof byok_known[0])

static MenuItem byok_items[NBYOK_KNOWN + 40];
static const Menu m_byok = { "BYOK", byok_items };

static const Menu m_main = { "Menú", (const MenuItem[]){
    { 'f', "Fitxer",               NULL, NULL, &m_file },
    { 'e', "Edita",                NULL, NULL, &m_edit },
    { 'c', "Cerca",                NULL, NULL, &m_find },
    { 'o', "Ortografia",           NULL, NULL, &m_spell },
    { 'v', "Vista",                NULL, NULL, &m_view },
    { 'b', "BYOK",                 NULL, NULL, &m_byok },
    { 'a', "Ajuda i configuració", NULL, NULL, &m_help },
    { 0 } } };

/* Les ordres externes poden canviar en recarregar la configuració */
static void build_byok(void)
{
    size_t n = 0, max = sizeof byok_items / sizeof byok_items[0] - 1;
    const Command *c;

    for (size_t i = 0; i < NBYOK_KNOWN; i++)
        if ((c = cmd_find(byok_known[i].cmd)) && !c->fn)
            byok_items[n++] = byok_known[i];
    for (size_t i = 0; i < cmd_count() && n < max; i++) {
        int known = 0;
        char key = 0;

        c = cmd_at(i);
        if (c->fn)
            continue;
        for (size_t k = 0; k < NBYOK_KNOWN && !known; k++)
            known = !strcmp(byok_known[k].cmd, c->name);
        if (known)
            continue;
        /* La primera lletra del nom que encara no fa servir cap opció */
        for (const char *p = c->name; *p && !key; p++) {
            key = *p >= 'a' && *p <= 'z' ? *p : 0;
            for (size_t k = 0; k < n && key; k++)
                if (byok_items[k].key == key)
                    key = 0;
        }
        byok_items[n++] = (MenuItem){ key, c->name, c->name, NULL, NULL };
    }
    byok_items[n] = (MenuItem){ 0 };
}

static int visible(const MenuItem *it)
{
    if (it->cmd)
        return cmd_find(it->cmd) != NULL;
    for (const MenuItem *s = it->sub->items; s->label; s++)
        if (visible(s))
            return 1;
    return 0;
}

#define MAX_DEPTH 4
#define MAX_ITEMS 48

typedef struct {
    const Menu     *menu;
    const MenuItem *items[MAX_ITEMS];
    int n, sel, top;
    int y, x, w, h;      /* caixa, vores incloses */
} Level;

static void open_level(Level *l, const Menu *m)
{
    l->menu = m;
    l->n = l->sel = l->top = 0;
    for (const MenuItem *it = m->items; it->label && l->n < MAX_ITEMS; it++)
        if (visible(it))
            l->items[l->n++] = it;
}

/* Escriu s a partir de la posició actual sense passar de maxw columnes.
 * Retorna les columnes ocupades. */
static int put_str(const char *s, int maxw)
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

static int str_width(const char *s)
{
    size_t len = strlen(s), i = 0;
    int col = 0;

    while (i < len) {
        wchar_t wc;

        i += utf8_decode(s, len, i, &wc);
        col += utf8_width(utf8_printable(wc) ? wc : L'?', col);
    }
    return col;
}

/* La drecera de l'opció: la primera tecla de l'ordre, o → si és un submenú.
 * Les opcions amb arguments no en tenen (la tecla l'executaria sense). */
static void shortcut(const MenuItem *it, char *out, size_t size)
{
    const Command *c;
    int key;

    out[0] = '\0';
    if (it->sub)
        snprintf(out, size, "→");
    else if (!it->args && (c = cmd_find(it->cmd)) && cmd_keys(c, &key, 1) == 1)
        key_describe(key, out, size);
}

/* Mida i posició de la caixa. El menú principal va a dalt, alineat amb el
 * text; cada submenú, a la dreta de l'opció que l'ha obert. */
static void layout(Editor *e, Level *l, const Level *parent)
{
    int labelw = 0, scw = 0;

    for (int i = 0; i < l->n; i++) {
        char sc[64];
        int w = str_width(l->items[i]->label);

        if (w > labelw)
            labelw = w;
        shortcut(l->items[i], sc, sizeof sc);
        if ((w = str_width(sc)) > scw)
            scw = w;
    }
    /* │ K  Etiqueta   Drecera │ */
    l->w = 2 + 4 + labelw + (scw ? 3 + scw : 0) + 1;
    if (l->w < str_width(l->menu->title) + 6)
        l->w = str_width(l->menu->title) + 6;
    l->h = l->n + 2;
    if (l->w > COLS)
        l->w = COLS;
    if (l->h > LINES - 1)
        l->h = LINES - 1 > 3 ? LINES - 1 : 3;

    if (!parent) {
        l->y = 1;
        l->x = e->margin > 1 ? e->margin - 1 : 0;
    } else {
        l->y = parent->y + 1 + parent->sel - parent->top;
        l->x = parent->x + parent->w - 1;
    }
    if (l->x + l->w > COLS)
        l->x = COLS - l->w;
    if (l->x < 0)
        l->x = 0;
    if (l->y + l->h > LINES - 1)
        l->y = LINES - 1 - l->h;
    if (l->y < 0)
        l->y = 0;

    /* L'opció triada sempre a la vista */
    if (l->sel < l->top)
        l->top = l->sel;
    if (l->sel >= l->top + l->h - 2)
        l->top = l->sel - (l->h - 3);
}

static void draw_level(const Level *l, int active)
{
    int iw = l->w - 2, rows = l->h - 2;

    /* Vores i títol */
    attrset(ui_attr(UI_MENU));
    mvaddch(l->y, l->x, ACS_ULCORNER);
    hline(ACS_HLINE, iw);
    mvaddch(l->y, l->x + l->w - 1, ACS_URCORNER);
    for (int i = 1; i <= rows; i++) {
        mvaddch(l->y + i, l->x, ACS_VLINE);
        mvhline(l->y + i, l->x + 1, ' ', iw);
        mvaddch(l->y + i, l->x + l->w - 1, ACS_VLINE);
    }
    mvaddch(l->y + l->h - 1, l->x, ACS_LLCORNER);
    hline(ACS_HLINE, iw);
    mvaddch(l->y + l->h - 1, l->x + l->w - 1, ACS_LRCORNER);
    if (iw > 4) {
        move(l->y, l->x + 2);
        addch(' ');
        attron(A_BOLD);
        put_str(l->menu->title, iw - 4);
        attroff(A_BOLD);
        addch(' ');
    }
    if (l->top > 0)
        mvaddch(l->y, l->x + l->w - 3, ACS_UARROW);
    if (l->top + rows < l->n)
        mvaddch(l->y + l->h - 1, l->x + l->w - 3, ACS_DARROW);

    for (int r = 0; r < rows && l->top + r < l->n; r++) {
        const MenuItem *it = l->items[l->top + r];
        int y = l->y + 1 + r, sel = l->top + r == l->sel;
        char sc[64];
        int scw, labelw;

        shortcut(it, sc, sizeof sc);
        scw = str_width(sc);
        labelw = iw - 5 - (scw ? 3 + scw : 0);
        if (labelw < 4) {   /* no hi cap la drecera */
            sc[0] = '\0';
            labelw = iw - 5;
        }
        if (sel && active)
            attrset(ui_attr(UI_SEL));
        else if (sel)
            attron(A_BOLD);
        mvhline(y, l->x + 1, ' ', iw);
        move(y, l->x + 2);
        if (it->key) {
            attron(A_BOLD);
            addch((chtype)towupper((wint_t)it->key));
            if (!(sel && !active))
                attroff(A_BOLD);
        } else {
            addch(' ');
        }
        addstr("  ");
        put_str(it->label, labelw > 0 ? labelw : 0);
        if (sc[0]) {
            move(y, l->x + l->w - 2 - scw);
            if (!sel)
                attron(A_DIM);
            put_str(sc, scw);
            attroff(A_DIM);
        }
        attrset(ui_attr(UI_MENU));
    }
    attrset(A_NORMAL);
}

static void draw_hint(void)
{
    move(LINES - 1, 0);
    clrtoeol();
    attron(A_DIM);
    put_str("↑↓ tria · Enter o la lletra entra · ← o Esc torna enrere", COLS - 1);
    attroff(A_DIM);
}

const Command *menu_choose(Editor *e, const char **args)
{
    Level lv[MAX_DEPTH];
    int depth = 0;

    *args = NULL;
    build_byok();
    open_level(&lv[0], &m_main);
    key_set_timeout(-1);
    for (;;) {
        Level *l;
        const MenuItem *it = NULL;
        int key;

        render(e);
        for (int i = 0; i <= depth; i++) {
            layout(e, &lv[i], i ? &lv[i - 1] : NULL);
            draw_level(&lv[i], i == depth);
        }
        draw_hint();
        l = &lv[depth];
        move(l->y + 1 + l->sel - l->top, l->x + 2);
        refresh();

        key = key_read();
        if (key < 0)
            continue;
        switch (key) {
        case KEY_FLAG_FN | KEY_UP:
            l->sel = l->sel > 0 ? l->sel - 1 : l->n - 1;
            break;
        case KEY_FLAG_FN | KEY_DOWN:
            l->sel = l->sel < l->n - 1 ? l->sel + 1 : 0;
            break;
        case KEY_FLAG_FN | KEY_HOME:
        case KEY_FLAG_FN | KEY_PPAGE:
            l->sel = 0;
            break;
        case KEY_FLAG_FN | KEY_END:
        case KEY_FLAG_FN | KEY_NPAGE:
            l->sel = l->n - 1;
            break;
        case KEY_FLAG_FN | KEY_LEFT:
        case 27:
            if (depth == 0)
                return NULL;
            depth--;
            break;
        case KEY_FLAG_FN | KEY_RIGHT:
            if (l->items[l->sel]->sub)
                it = l->items[l->sel];
            break;
        case '\n':
            it = l->items[l->sel];
            break;
        default:
            if (key & (KEY_FLAG_FN | KEY_FLAG_ALT) || !utf8_printable((wchar_t)key))
                break;
            for (int i = 0; i < l->n && !it; i++) {
                if (l->items[i]->key && (wint_t)l->items[i]->key == towlower((wint_t)key)) {
                    l->sel = i;
                    it = l->items[i];
                }
            }
        }
        if (!it)
            continue;
        if (!it->sub) {
            *args = it->args;
            return cmd_find(it->cmd);
        }
        if (depth + 1 < MAX_DEPTH)
            open_level(&lv[++depth], it->sub);
    }
}
