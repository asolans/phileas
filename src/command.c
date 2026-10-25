#include "command.h"
#include "keys.h"
#include "render.h"
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

    if (!prompt(e, "Anar a la línia: ", buf, sizeof buf) || !buf[0])
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
    if (!prompt(e, "Desa com: ", path, sizeof path) || !path[0]) {
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

    if (!prompt(e, "Ordre: ", name, sizeof name) || !name[0])
        return;
    if (!(c = cmd_find(name))) {
        ed_msg(e, "Ordre desconeguda: %s", name);
        return;
    }
    c->fn(e);
}

static const Command commands[] = {
    { "moure-amunt",       c_up,         "Puja una línia de pantalla" },
    { "moure-avall",       c_down,       "Baixa una línia de pantalla" },
    { "moure-esquerra",    c_left,       "Un caràcter a l'esquerra" },
    { "moure-dreta",       c_right,      "Un caràcter a la dreta" },
    { "inici-linia",       c_row_start,  "Inici de la línia de pantalla" },
    { "fi-linia",          c_row_end,    "Final de la línia de pantalla" },
    { "pagina-amunt",      c_page_up,    "Una pantalla amunt" },
    { "pagina-avall",      c_page_down,  "Una pantalla avall" },
    { "paraula-anterior",  c_word_prev,  "Inici de la paraula anterior" },
    { "paraula-seguent",   c_word_next,  "Inici de la paraula següent" },
    { "paragraf-anterior", c_par_prev,   "Inici del paràgraf anterior" },
    { "paragraf-seguent",  c_par_next,   "Inici del paràgraf següent" },
    { "inici-document",    c_doc_start,  "Inici del document" },
    { "fi-document",       c_doc_end,    "Final del document" },
    { "anar-a-linia",      c_goto_line,  "Salta a una línia (paràgraf) pel número" },

    { "seleccio-amunt",            c_sel_up,         "Amplia la selecció una línia amunt" },
    { "seleccio-avall",            c_sel_down,       "Amplia la selecció una línia avall" },
    { "seleccio-esquerra",         c_sel_left,       "Amplia la selecció un caràcter a l'esquerra" },
    { "seleccio-dreta",            c_sel_right,      "Amplia la selecció un caràcter a la dreta" },
    { "seleccio-inici-linia",      c_sel_row_start,  "Amplia la selecció fins a l'inici de la línia" },
    { "seleccio-fi-linia",         c_sel_row_end,    "Amplia la selecció fins al final de la línia" },
    { "seleccio-pagina-amunt",     c_sel_page_up,    "Amplia la selecció una pantalla amunt" },
    { "seleccio-pagina-avall",     c_sel_page_down,  "Amplia la selecció una pantalla avall" },
    { "seleccio-paraula-anterior", c_sel_word_prev,  "Amplia la selecció fins a la paraula anterior" },
    { "seleccio-paraula-seguent",  c_sel_word_next,  "Amplia la selecció fins a la paraula següent" },
    { "seleccio-paragraf-anterior", c_sel_par_prev,  "Amplia la selecció fins al paràgraf anterior" },
    { "seleccio-paragraf-seguent", c_sel_par_next,   "Amplia la selecció fins al paràgraf següent" },
    { "seleccio-inici-document",   c_sel_doc_start,  "Amplia la selecció fins a l'inici del document" },
    { "seleccio-fi-document",      c_sel_doc_end,    "Amplia la selecció fins al final del document" },
    { "marca",                     c_mark,           "Comença o cancel·la una selecció" },
    { "seleccionar-tot",           c_select_all,     "Selecciona tot el document" },
    { "cancellar",                 c_cancel,         "Cancel·la la selecció" },

    { "nova-linia",                c_newline,        "Parteix la línia; continua llistes i cites" },
    { "tabulador",                 c_tab,            "Insereix un tabulador" },
    { "esborrar-enrere",           c_backspace,      "Esborra el caràcter anterior" },
    { "esborrar-endavant",         c_delete,         "Esborra el caràcter següent" },
    { "esborrar-paraula-anterior", c_delete_word_prev, "Esborra fins a l'inici de la paraula" },
    { "esborrar-paraula-seguent",  c_delete_word_next, "Esborra fins a la paraula següent" },
    { "tallar",                    c_cut,            "Talla la selecció" },
    { "copiar",                    c_copy,           "Copia la selecció" },
    { "enganxar",                  c_paste,          "Enganxa el porta-retalls" },

    { "centrar",           c_center,     "Centra o no la columna de text" },
    { "desar",             c_save,       "Desa el document" },
    { "desar-com",         c_save_as,    "Desa el document amb un altre nom" },
    { "sortir",            c_quit,       "Surt de Phileas" },
    { "redibuixar",        c_redraw,     "Torna a dibuixar la pantalla" },
    { "paleta",            c_palette,    "Executa una ordre pel nom" },
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
