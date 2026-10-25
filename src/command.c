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

static void c_up(Editor *e)         { ed_move_up(e); }
static void c_down(Editor *e)       { ed_move_down(e); }
static void c_left(Editor *e)       { ed_move_left(e); }
static void c_right(Editor *e)      { ed_move_right(e); }
static void c_row_start(Editor *e)  { ed_row_start(e); }
static void c_row_end(Editor *e)    { ed_row_end(e); }
static void c_page_up(Editor *e)    { ed_page_up(e); }
static void c_page_down(Editor *e)  { ed_page_down(e); }
static void c_word_prev(Editor *e)  { ed_word_prev(e); }
static void c_word_next(Editor *e)  { ed_word_next(e); }
static void c_par_prev(Editor *e)   { ed_par_prev(e); }
static void c_par_next(Editor *e)   { ed_par_next(e); }
static void c_doc_start(Editor *e)  { ed_doc_start(e); }
static void c_doc_end(Editor *e)    { ed_doc_end(e); }

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
