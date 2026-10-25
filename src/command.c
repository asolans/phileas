#include "command.h"
#include "keys.h"
#include "render.h"
#include "search.h"
#include "tui.h"
#include "util.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* Moviment: sense Maj cancel·la la selecció feta amb Maj+fletxes, però
 * amplia la feta amb Ctrl+Espai. Amb Maj, la comença o l'amplia. */
#define MOVE_COMMANDS(name, fn)                     \
    static void c_##name(Editor *e)                 \
    {                                               \
        if (e->sel_mode == SEL_SHIFT)               \
            ed_sel_clear(e);                        \
        fn(e);                                      \
    }                                               \
    static void c_sel_##name(Editor *e)             \
    {                                               \
        if (e->sel_mode == SEL_NONE)                \
            ed_sel_start(e, SEL_SHIFT);             \
        fn(e);                                      \
    }

MOVE_COMMANDS(up, ed_move_up)
MOVE_COMMANDS(down, ed_move_down)
MOVE_COMMANDS(left, ed_move_left)
MOVE_COMMANDS(right, ed_move_right)
MOVE_COMMANDS(row_start, ed_row_start)
MOVE_COMMANDS(row_end, ed_row_end)
MOVE_COMMANDS(page_up, ed_page_up)
MOVE_COMMANDS(page_down, ed_page_down)
MOVE_COMMANDS(word_prev, ed_word_prev)
MOVE_COMMANDS(word_next, ed_word_next)
MOVE_COMMANDS(par_prev, ed_par_prev)
MOVE_COMMANDS(par_next, ed_par_next)
MOVE_COMMANDS(doc_start, ed_doc_start)
MOVE_COMMANDS(doc_end, ed_doc_end)

static void c_newline(Editor *e)          { ed_newline(e); }
static void c_tab(Editor *e)              { ed_insert(e, "\t", 1); }
static void c_backspace(Editor *e)        { ed_backspace(e); }
static void c_delete(Editor *e)           { ed_delete(e); }
static void c_delete_word_prev(Editor *e) { ed_delete_word_prev(e); }
static void c_delete_word_next(Editor *e) { ed_delete_word_next(e); }

static void c_mark(Editor *e)
{
    if (e->sel_mode != SEL_NONE) {
        ed_sel_clear(e);
        ed_msg(e, "Selecció cancel·lada");
    } else {
        ed_sel_start(e, SEL_MARK);
        ed_msg(e, "Marca posada: mou el cursor per seleccionar (Ctrl+Espai o Esc per cancel·lar)");
    }
}

static void c_select_all(Editor *e)
{
    ed_select_all(e);
}

static void c_cancel(Editor *e)
{
    ed_sel_clear(e);
}

static void c_copy(Editor *e)
{
    size_t n = ed_copy(e);

    if (n)
        ed_msg(e, "Copiat (%zu caràcters)", n);
    else
        ed_msg(e, "No hi ha res seleccionat");
}

static void c_cut(Editor *e)
{
    size_t n = ed_cut(e);

    if (n)
        ed_msg(e, "Tallat (%zu caràcters)", n);
    else
        ed_msg(e, "No hi ha res seleccionat");
}

static void c_paste(Editor *e)
{
    if (!ed_paste(e))
        ed_msg(e, "El porta-retalls és buit");
}

static void c_center(Editor *e)
{
    e->center = !e->center;
    if (!e->center)
        ed_msg(e, "Columna a l'esquerra");
    else if (COLS > e->columns)
        ed_msg(e, "Columna centrada");
    else
        ed_msg(e, "No hi ha marge per centrar (terminal %d < text %d)", COLS, e->columns);
}

static void c_goto_line(Editor *e)
{
    char buf[32] = "";
    char *end;
    unsigned long n;

    if (!prompt(e, "Anar a la línia: ", buf, sizeof buf, NULL) || !buf[0])
        return;
    n = strtoul(buf, &end, 10);
    if (*end) {
        ed_msg(e, "Número de línia no vàlid: %s", buf);
        return;
    }
    ed_goto_line(e, n);
}

static void save_to(Editor *e, const char *path)
{
    if (buf_save(&e->buf, path) < 0) {
        ed_msg(e, "No s'ha pogut desar %s: %s", path, strerror(errno));
        return;
    }
    if (!e->filename || strcmp(e->filename, path)) {
        char *name = xstrdup(path);
        free(e->filename);
        e->filename = name;
    }
    ed_msg(e, "Desat: %s (%zu %s)", path, e->buf.nlines,
           e->buf.nlines == 1 ? "línia" : "línies");
}

static void c_save_as(Editor *e)
{
    char path[PATH_MAX];
    struct stat st;

    snprintf(path, sizeof path, "%s", e->filename ? e->filename : "");
    if (!prompt(e, "Desa com: ", path, sizeof path, NULL) || !path[0]) {
        ed_msg(e, "No s'ha desat");
        return;
    }
    if ((!e->filename || strcmp(path, e->filename)) && stat(path, &st) == 0 &&
        !prompt_yesno(e, "El fitxer ja existeix. El vols sobreescriure?")) {
        ed_msg(e, "No s'ha desat");
        return;
    }
    save_to(e, path);
}

static void c_save(Editor *e)
{
    if (e->filename)
        save_to(e, e->filename);
    else
        c_save_as(e);
}

static void c_quit(Editor *e)
{
    if (e->buf.modified && !e->quit_confirm) {
        ed_msg(e, "Hi ha canvis sense desar. Torna a prémer Ctrl+Q per sortir sense desar.");
        e->quit_confirm = 1;
        return;
    }
    e->quit = 1;
}

static void c_redraw(Editor *e)
{
    (void)e;
    clearok(curscr, TRUE);
}

static void c_palette(Editor *e)
{
    char name[64] = "";
    const Command *c;

    if (!prompt(e, "Ordre: ", name, sizeof name, NULL) || !name[0])
        return;
    if (!(c = cmd_find(name))) {
        ed_msg(e, "Ordre desconeguda: %s", name);
        return;
    }
    c->fn(e);
}

static void c_undo(Editor *e)
{
    if (!ed_undo(e))
        ed_msg(e, "No hi ha res per desfer");
}

static void c_redo(Editor *e)
{
    if (!ed_redo(e))
        ed_msg(e, "No hi ha res per refer");
}

/* ---- Cerca ---- */

static Pos search_origin;   /* on era el cursor en començar la cerca */
static char find_label[sizeof ((Editor *)0)->search + 32];

/* Etiqueta de la pregunta: mostra l'última cerca, que es reutilitza si es
 * prem Enter sense escriure res, i avisa si no es troba res */
static void set_find_label(Editor *e, const char *what, int found)
{
    if (e->search[0])
        snprintf(find_label, sizeof find_label, "%s%s [%s]: ", what,
                 found ? "" : " (no trobat)", e->search);
    else
        snprintf(find_label, sizeof find_label, "%s%s: ", what, found ? "" : " (no trobat)");
}

static int find_and_select(Editor *e, Pos from, const char *pat, int dir)
{
    Pos a, z;
    int wrapped;

    if (!search_find(&e->buf, from, pat, dir, &a, &z, &wrapped)) {
        ed_sel_clear(e);
        if (pat[0])
            ed_msg(e, "No s'ha trobat «%s»", pat);
        return 0;
    }
    ed_select(e, a, z);
    if (wrapped)
        ed_msg(e, dir > 0 ? "Cerca des del principi del document" : "Cerca des del final del document");
    return 1;
}

static void search_changed(Editor *e, const char *text)
{
    int found;

    e->cy = search_origin.line;
    e->cx = search_origin.col;
    found = !text[0] || find_and_select(e, search_origin, text, 1);
    if (!found) {
        e->cy = search_origin.line;
        e->cx = search_origin.col;
    }
    e->msg[0] = '\0';
    set_find_label(e, "Cerca", found);
}

static void c_find(Editor *e)
{
    char pat[sizeof e->search] = "";

    search_origin = (Pos){ e->cy, e->cx };
    ed_sel_clear(e);
    set_find_label(e, "Cerca", 1);
    if (!prompt(e, find_label, pat, sizeof pat, search_changed)) {
        /* Esc: el cursor torna on era */
        ed_sel_clear(e);
        e->cy = search_origin.line;
        e->cx = search_origin.col;
        e->want_x = -1;
        e->msg[0] = '\0';
        return;
    }
    if (pat[0]) {
        snprintf(e->search, sizeof e->search, "%s", pat);
    } else if (e->search[0]) {
        /* Enter sense escriure res: torna a cercar l'última */
        find_and_select(e, search_origin, e->search, 1);
    }
}

static void c_find_next(Editor *e)
{
    if (!e->search[0]) {
        c_find(e);
        return;
    }
    find_and_select(e, (Pos){ e->cy, e->cx }, e->search, 1);
}

static void c_find_prev(Editor *e)
{
    Pos a, z, from = { e->cy, e->cx };

    if (!e->search[0]) {
        c_find(e);
        return;
    }
    if (ed_selection(e, &a, &z))
        from = a;
    find_and_select(e, from, e->search, -1);
}

static void c_replace(Editor *e)
{
    char pat[sizeof e->search] = "", rep[256] = "";
    Pos origin = { e->cy, e->cx }, from = origin, a, z;
    size_t plen, rlen, count = 0;
    int wrapped, passed = 0, all = 0, answer;

    set_find_label(e, "Substitueix", 1);
    if (!prompt(e, find_label, pat, sizeof pat, NULL))
        return;
    if (!pat[0])
        snprintf(pat, sizeof pat, "%s", e->search);
    if (!pat[0])
        return;
    snprintf(e->search, sizeof e->search, "%s", pat);
    if (!prompt(e, "Substitueix per: ", rep, sizeof rep, NULL))
        return;
    rlen = strlen(rep);

    while (search_find(&e->buf, from, pat, 1, &a, &z, &wrapped)) {
        /* S'acaba en tornar al punt de partida després de donar la volta */
        passed |= wrapped;
        if (passed && (a.line > origin.line || (a.line == origin.line && a.col >= origin.col)))
            break;
        if (!all) {
            ed_select(e, a, z);
            answer = prompt_choice(e, "Substituir? (s)í, (n)o, (t)ots, Esc: acabar", "snt");
            if (answer == 0)
                break;
            if (answer == 'n') {
                from = z;
                continue;
            }
            all = answer == 't';
        }
        plen = a.line == z.line ? z.col - a.col : 0;
        buf_delete(&e->buf, a, z);
        from = buf_insert(&e->buf, a, rep, rlen);
        if (a.line == origin.line && a.col < origin.col)
            origin.col = origin.col + rlen - plen;
        count++;
    }
    ed_sel_clear(e);
    if (count)
        e->cy = from.line, e->cx = from.col;
    e->want_x = -1;
    ed_msg(e, count == 1 ? "1 substitució" : "%zu substitucions", count);
}

static void c_autosave(Editor *e)
{
    if (e->autosave) {
        e->autosave_last = e->autosave;
        e->autosave = 0;
        ed_msg(e, "Autodesat desactivat");
    } else {
        e->autosave = e->autosave_last > 0 ? e->autosave_last : 30;
        ed_msg(e, "Autodesat després de %d s sense escriure", e->autosave);
    }
}

static const Command commands[] = {
    { "moure-amunt",       c_up,         "Puja una línia de pantalla", ACT_NONE },
    { "moure-avall",       c_down,       "Baixa una línia de pantalla", ACT_NONE },
    { "moure-esquerra",    c_left,       "Un caràcter a l'esquerra", ACT_NONE },
    { "moure-dreta",       c_right,      "Un caràcter a la dreta", ACT_NONE },
    { "inici-linia",       c_row_start,  "Inici de la línia de pantalla", ACT_NONE },
    { "fi-linia",          c_row_end,    "Final de la línia de pantalla", ACT_NONE },
    { "pagina-amunt",      c_page_up,    "Una pantalla amunt", ACT_NONE },
    { "pagina-avall",      c_page_down,  "Una pantalla avall", ACT_NONE },
    { "paraula-anterior",  c_word_prev,  "Inici de la paraula anterior", ACT_NONE },
    { "paraula-seguent",   c_word_next,  "Inici de la paraula següent", ACT_NONE },
    { "paragraf-anterior", c_par_prev,   "Inici del paràgraf anterior", ACT_NONE },
    { "paragraf-seguent",  c_par_next,   "Inici del paràgraf següent", ACT_NONE },
    { "inici-document",    c_doc_start,  "Inici del document", ACT_NONE },
    { "fi-document",       c_doc_end,    "Final del document", ACT_NONE },
    { "anar-a-linia",      c_goto_line,  "Salta a una línia (paràgraf) pel número", ACT_NONE },

    { "seleccio-amunt",            c_sel_up,         "Amplia la selecció una línia amunt", ACT_NONE },
    { "seleccio-avall",            c_sel_down,       "Amplia la selecció una línia avall", ACT_NONE },
    { "seleccio-esquerra",         c_sel_left,       "Amplia la selecció un caràcter a l'esquerra", ACT_NONE },
    { "seleccio-dreta",            c_sel_right,      "Amplia la selecció un caràcter a la dreta", ACT_NONE },
    { "seleccio-inici-linia",      c_sel_row_start,  "Amplia la selecció fins a l'inici de la línia", ACT_NONE },
    { "seleccio-fi-linia",         c_sel_row_end,    "Amplia la selecció fins al final de la línia", ACT_NONE },
    { "seleccio-pagina-amunt",     c_sel_page_up,    "Amplia la selecció una pantalla amunt", ACT_NONE },
    { "seleccio-pagina-avall",     c_sel_page_down,  "Amplia la selecció una pantalla avall", ACT_NONE },
    { "seleccio-paraula-anterior", c_sel_word_prev,  "Amplia la selecció fins a la paraula anterior", ACT_NONE },
    { "seleccio-paraula-seguent",  c_sel_word_next,  "Amplia la selecció fins a la paraula següent", ACT_NONE },
    { "seleccio-paragraf-anterior", c_sel_par_prev,  "Amplia la selecció fins al paràgraf anterior", ACT_NONE },
    { "seleccio-paragraf-seguent", c_sel_par_next,   "Amplia la selecció fins al paràgraf següent", ACT_NONE },
    { "seleccio-inici-document",   c_sel_doc_start,  "Amplia la selecció fins a l'inici del document", ACT_NONE },
    { "seleccio-fi-document",      c_sel_doc_end,    "Amplia la selecció fins al final del document", ACT_NONE },
    { "marca",                     c_mark,           "Comença o cancel·la una selecció", ACT_NONE },
    { "seleccionar-tot",           c_select_all,     "Selecciona tot el document", ACT_NONE },
    { "cancellar",                 c_cancel,         "Cancel·la la selecció", ACT_NONE },

    { "nova-linia",                c_newline,        "Parteix la línia; continua llistes i cites", ACT_NONE },
    { "tabulador",                 c_tab,            "Insereix un tabulador", ACT_NONE },
    { "esborrar-enrere",           c_backspace,      "Esborra el caràcter anterior", ACT_BACKSPACE },
    { "esborrar-endavant",         c_delete,         "Esborra el caràcter següent", ACT_DELETE },
    { "esborrar-paraula-anterior", c_delete_word_prev, "Esborra fins a l'inici de la paraula", ACT_NONE },
    { "esborrar-paraula-seguent",  c_delete_word_next, "Esborra fins a la paraula següent", ACT_NONE },
    { "tallar",                    c_cut,            "Talla la selecció", ACT_NONE },
    { "copiar",                    c_copy,           "Copia la selecció", ACT_NONE },
    { "enganxar",                  c_paste,          "Enganxa el porta-retalls", ACT_NONE },

    { "centrar",           c_center,     "Centra o no la columna de text", ACT_NONE },
    { "desar",             c_save,       "Desa el document", ACT_NONE },
    { "desar-com",         c_save_as,    "Desa el document amb un altre nom", ACT_NONE },
    { "sortir",            c_quit,       "Surt de Phileas", ACT_NONE },
    { "redibuixar",        c_redraw,     "Torna a dibuixar la pantalla", ACT_NONE },
    { "paleta",            c_palette,    "Executa una ordre pel nom", ACT_NONE },

    { "desfer",            c_undo,       "Desfà l'últim canvi", ACT_NONE },
    { "refer",             c_redo,       "Refà el que s'ha desfet", ACT_NONE },
    { "cercar",            c_find,       "Cerca un text (mentre s'escriu)", ACT_NONE },
    { "cercar-seguent",    c_find_next,  "Següent coincidència", ACT_NONE },
    { "cercar-anterior",   c_find_prev,  "Coincidència anterior", ACT_NONE },
    { "substituir",        c_replace,    "Substitueix un text, amb confirmació", ACT_NONE },
    { "autodesat",         c_autosave,   "Activa o desactiva l'autodesat", ACT_NONE },
};

static const char *const default_bindings[][2] = {
    { "up", "moure-amunt" },          { "down", "moure-avall" },
    { "left", "moure-esquerra" },     { "right", "moure-dreta" },
    { "home", "inici-linia" },        { "end", "fi-linia" },
    { "pgup", "pagina-amunt" },       { "pgdn", "pagina-avall" },
    { "ctrl-left", "paraula-anterior" },  { "alt-b", "paraula-anterior" },
    { "ctrl-right", "paraula-seguent" },  { "alt-f", "paraula-seguent" },
    { "ctrl-up", "paragraf-anterior" },   { "alt-p", "paragraf-anterior" },
    { "ctrl-down", "paragraf-seguent" },  { "alt-n", "paragraf-seguent" },
    { "ctrl-home", "inici-document" },    { "alt-<", "inici-document" },
    { "ctrl-end", "fi-document" },        { "alt->", "fi-document" },
    { "alt-l", "anar-a-linia" },

    { "shift-up", "seleccio-amunt" },     { "shift-down", "seleccio-avall" },
    { "shift-left", "seleccio-esquerra" }, { "shift-right", "seleccio-dreta" },
    { "shift-home", "seleccio-inici-linia" }, { "shift-end", "seleccio-fi-linia" },
    { "shift-pgup", "seleccio-pagina-amunt" }, { "shift-pgdn", "seleccio-pagina-avall" },
    { "ctrl-shift-left", "seleccio-paraula-anterior" },
    { "ctrl-shift-right", "seleccio-paraula-seguent" },
    { "ctrl-space", "marca" },
    { "ctrl-a", "seleccionar-tot" },
    { "esc", "cancellar" },

    { "enter", "nova-linia" },
    { "tab", "tabulador" },
    { "backspace", "esborrar-enrere" },
    { "delete", "esborrar-endavant" },
    { "ctrl-w", "esborrar-paraula-anterior" }, { "ctrl-h", "esborrar-paraula-anterior" },
    { "alt-d", "esborrar-paraula-seguent" },   { "ctrl-delete", "esborrar-paraula-seguent" },
    { "ctrl-x", "tallar" },  { "ctrl-c", "copiar" },  { "ctrl-v", "enganxar" },

    { "alt-c", "centrar" },
    { "ctrl-s", "desar" },            { "alt-s", "desar-com" },
    { "ctrl-q", "sortir" },
    { "ctrl-l", "redibuixar" },
    { "ctrl-p", "paleta" },

    { "ctrl-z", "desfer" },           { "ctrl-y", "refer" },
    { "ctrl-f", "cercar" },
    { "ctrl-g", "cercar-seguent" },   { "f3", "cercar-seguent" },
    { "alt-g", "cercar-anterior" },   { "shift-f3", "cercar-anterior" },
    { "ctrl-r", "substituir" },
};

#define MAX_BINDINGS 256

static struct {
    int key;
    const Command *cmd;
} bindings[MAX_BINDINGS];
static size_t nbindings;

const Command *cmd_find(const char *name)
{
    for (size_t i = 0; i < sizeof commands / sizeof commands[0]; i++)
        if (!strcmp(commands[i].name, name))
            return &commands[i];
    return NULL;
}

const Command *cmd_for_key(int key)
{
    for (size_t i = 0; i < nbindings; i++)
        if (bindings[i].key == key)
            return bindings[i].cmd;
    return NULL;
}

int cmd_bind(const char *keyname, const char *cmdname)
{
    int key = key_parse(keyname);
    const Command *c = cmd_find(cmdname);

    if (key < 0 || !c)
        return -1;
    for (size_t i = 0; i < nbindings; i++) {
        if (bindings[i].key == key) {
            bindings[i].cmd = c;
            return 0;
        }
    }
    if (nbindings == MAX_BINDINGS)
        return -1;
    bindings[nbindings].key = key;
    bindings[nbindings++].cmd = c;
    return 0;
}

void cmd_bind_defaults(void)
{
    for (size_t i = 0; i < sizeof default_bindings / sizeof default_bindings[0]; i++) {
        if (!cmd_find(default_bindings[i][1]))
            die("drecera per defecte amb una ordre inexistent: %s", default_bindings[i][1]);
        /* Si el terminal no té la tecla (p. ex. Ctrl+fletxes a la consola
         * de Linux), en queda l'alternativa amb Alt */
        cmd_bind(default_bindings[i][0], default_bindings[i][1]);
    }
}
