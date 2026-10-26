#include "command.h"
#include "complete.h"
#include "config.h"
#include "filebrowser.h"
#include "help.h"
#include "keys.h"
#include "render.h"
#include "search.h"
#include "spell.h"
#include "utf8.h"
#include "tui.h"
#include "util.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

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
    else if (COLS - e->margin_left - e->margin_right > e->columns)
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

/* Abans de canviar de document: si hi ha canvis, pregunta si cal desar-los.
 * Retorna 0 si l'usuari cancel·la o el desat falla. */
static int confirm_discard(Editor *e)
{
    int c;

    if (!e->buf.modified)
        return 1;
    c = prompt_choice(e, "Hi ha canvis sense desar. Els vols desar? (s/n, Esc cancel·la) ", "sn");
    if (c == 0) {
        ed_msg(e, "Cancel·lat");
        return 0;
    }
    if (c == 'n')
        return 1;
    c_save(e);
    return !e->buf.modified;
}

/* 1 si el fitxer sembla binari (té bytes nuls al principi). */
static int looks_binary(const char *path)
{
    char data[8192];
    FILE *f = fopen(path, "rb");
    size_t n;

    if (!f)
        return 0;
    n = fread(data, 1, sizeof data, f);
    fclose(f);
    return memchr(data, '\0', n) != NULL;
}

static void c_open(Editor *e)
{
    char path[PATH_MAX];

    if (!file_browser(e, path, sizeof path))
        return;
    if (e->filename && !strcmp(path, e->filename)) {
        ed_msg(e, "Ja és el document obert");
        return;
    }
    if (looks_binary(path)) {
        ed_msg(e, "%s sembla un fitxer binari: no s'obre", path);
        return;
    }
    if (!confirm_discard(e))
        return;
    e->msg[0] = '\0';
    if (ed_open(e, path) < 0) {
        ed_msg(e, "No s'ha pogut obrir %s: %s", path, strerror(errno));
        return;
    }
    if (!e->msg[0])
        ed_msg(e, "Obert: %s (%zu %s)", path, e->buf.nlines,
               e->buf.nlines == 1 ? "línia" : "línies");
}

static void c_new(Editor *e)
{
    if (!confirm_discard(e))
        return;
    ed_open(e, NULL);
    ed_msg(e, "Document nou");
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

/* Tab a la paleta: completa el nom de l'ordre i, si n'hi ha més d'una,
 * les mostra. Els arguments no es completen. */
static void palette_complete(Editor *e, char *buf, size_t size, char *hint, size_t hintsz)
{
    size_t n = cmd_count(), matches, len, used;
    const char **names = xmalloc(n * sizeof *names);
    char *word = buf + strspn(buf, " ");

    (void)e;
    hint[0] = '\0';
    if (strchr(word, ' ')) {
        free(names);
        return;
    }
    for (size_t i = 0; i < n; i++)
        names[i] = cmd_at(i)->name;
    matches = complete_word(word, size - (size_t)(word - buf), names, n);
    len = strlen(word);
    if (matches == 0) {
        snprintf(hint, hintsz, "  (cap ordre comença així)");
    } else if (matches == 1) {
        if (strlen(buf) + 1 < size)
            strcat(buf, " ");
    } else {
        used = (size_t)snprintf(hint, hintsz, " ");
        for (size_t i = 0; i < n && used + 1 < hintsz; i++)
            if (!strncmp(names[i], word, len))
                used += (size_t)snprintf(hint + used, hintsz - used, " %s", names[i]);
    }
    free(names);
}

/* Ctrl+P: el nom de l'ordre i, separats per espais, els arguments (només
 * per a les ordres externes: «lletra +») */
static void c_palette(Editor *e)
{
    char line[256] = "";
    char *name, *args;
    const Command *c;

    if (!prompt_complete(e, "Ordre: ", line, sizeof line, palette_complete))
        return;
    name = line + strspn(line, " ");
    if (!*name)
        return;
    args = name + strcspn(name, " ");
    if (*args)
        *args++ = '\0';
    args += strspn(args, " ");
    if (!(c = cmd_find(name))) {
        ed_msg(e, "Ordre desconeguda: %s", name);
        return;
    }
    cmd_run(e, c, args);
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

static void c_typewriter(Editor *e)
{
    e->tw_mode = (e->tw_mode + 1) % 3;
    if (e->tw_mode == TW_SALT)
        ed_msg(e, "Màquina d'escriure: salt (en passar el %d %% de l'alçada, el cursor torna al %d %%)",
               e->tw_limit, e->tw_anchor);
    else if (e->tw_mode == TW_FIXA)
        ed_msg(e, "Màquina d'escriure: línia fixa (al %d %% de l'alçada)", e->tw_anchor);
    else
        ed_msg(e, "Desplaçament clàssic (fins a baix de tot)");
}

static void c_reload_config(Editor *e)
{
    char msg[256], path[1100];
    int n;

    cmd_bind_defaults();
    cmd_extern_clear();
    n = config_load(e, CONFIG_SYSTEM, NULL, msg, sizeof msg);
    render_colors(e);
    config_user_path(path, sizeof path);
    if (n)
        ed_msg(e, "Configuració: %s", msg);
    else
        ed_msg(e, "Configuració recarregada (%s)", path);
}

static void c_help(Editor *e)
{
    help_show(e);
}

static void c_colors(Editor *e)
{
    e->colors = !e->colors;
    ed_msg(e, e->colors ? "Colors de Markdown activats" : "Colors de Markdown desactivats");
}

/* ---- Ortografia ---- */

/* El diccionari es carrega la primera vegada que es fa servir: en una
 * Raspberry Pi petita pot trigar uns segons i ocupa memòria. */
static int need_dict(Editor *e)
{
    if (spell_lang() && !strcmp(e->dict_loaded, e->dict))
        return 1;
    if (!spell_loaded(e->dict)) {
        ed_msg(e, "Carregant el diccionari %s…", e->dict);
        render(e);
        refresh();
    }
    if (spell_open(e->dict) == 0) {
        snprintf(e->dict_loaded, sizeof e->dict_loaded, "%s", e->dict);
        e->msg[0] = '\0';
        return 1;
    }
    ed_msg(e, "No s'ha pogut carregar el diccionari: %s", spell_error());
    return 0;
}

/* Canvia el diccionari triat; si no es pot carregar, torna a l'anterior. */
static int set_dict(Editor *e, const char *name)
{
    char old[sizeof e->dict];

    snprintf(old, sizeof old, "%s", e->dict);
    snprintf(e->dict, sizeof e->dict, "%s", name);
    if (!e->spell || need_dict(e))
        return 1;
    snprintf(e->dict, sizeof e->dict, "%s", old);
    return 0;
}

/* Estils de Markdown de la línia y, per no revisar el codi ni els marcadors
 * (cal alliberar-los). */
static unsigned char *styles_of(Editor *e, size_t y)
{
    Line *l = &e->buf.lines[y];
    unsigned char *st = xmalloc(l->len + 1);

    buf_scan(&e->buf);
    if (e->buf.plain)
        memset(st, 0, l->len);
    else
        md_style(l->text, l->len, l->kind, 0, st);
    return st;
}

/* Error ortogràfic següent a partir de (y, x), fent la volta al document. */
static int find_error(Editor *e, size_t y, size_t x, Pos *a, Pos *z)
{
    for (size_t k = 0; k <= e->buf.nlines; k++) {
        size_t line = (y + k) % e->buf.nlines, from = k == 0 ? x : 0, s, t;
        Line *l = &e->buf.lines[line];
        unsigned char *st = styles_of(e, line);
        int found = spell_next_error(l->text, l->len, st, from, &s, &t);

        free(st);
        /* A l'última volta, només fins on s'ha començat */
        if (found && (k < e->buf.nlines || s < x)) {
            a->line = z->line = line;
            a->col = s;
            z->col = t;
            return 1;
        }
    }
    return 0;
}

static void c_spell_next(Editor *e)
{
    Pos a, z, from;
    Line *l;

    if (!need_dict(e))
        return;
    e->spell = 1;   /* revisar implica veure els errors */
    if (!ed_selection(e, &a, &from)) {
        from.line = e->cy;
        from.col = e->cx;
    }
    if (!find_error(e, from.line, from.col, &a, &z)) {
        ed_msg(e, "Cap error ortogràfic");
        return;
    }
    if (a.line < from.line || (a.line == from.line && a.col < from.col))
        ed_msg(e, "(des del principi) ");
    else
        e->msg[0] = '\0';
    ed_select(e, a, z);
    l = &e->buf.lines[a.line];
    snprintf(e->msg + strlen(e->msg), sizeof e->msg - strlen(e->msg),
             "«%.*s»: Alt+Enter suggeriments · Maj+F7 següent", (int)(z.col - a.col), l->text + a.col);
}

/* Paraula seleccionada (dins d'una línia) o sota el cursor. */
static int word_at_cursor(Editor *e, Pos *a, Pos *z)
{
    Line *l = &e->buf.lines[e->cy];
    size_t from = 0, s, t;

    if (ed_selection(e, a, z))
        return a->line == z->line && z->col > a->col;
    while (spell_next_word(l->text, l->len, from, &s, &t)) {
        if (s <= e->cx && e->cx <= t) {
            a->line = z->line = e->cy;
            a->col = s;
            z->col = t;
            return 1;
        }
        from = t;
    }
    return 0;
}

static void c_suggest(Editor *e)
{
    Pos a, z;
    char word[256], question[512], keys[16] = "";
    char **list;
    int n, shown = 0, c;
    size_t len, nk = 0;

    if (!need_dict(e))
        return;
    if (!word_at_cursor(e, &a, &z)) {
        ed_msg(e, "No hi ha cap paraula al cursor");
        return;
    }
    len = z.col - a.col;
    if (len >= sizeof word)
        len = sizeof word - 1;
    memcpy(word, e->buf.lines[a.line].text + a.col, len);
    word[len] = '\0';
    if (spell_check(word, len)) {
        ed_msg(e, "«%s» és correcta", word);
        return;
    }
    ed_msg(e, "Buscant suggeriments…");
    render(e);
    refresh();
    n = spell_suggest(word, len, &list);

    /* Tants suggeriments com càpiguen a la línia (fins a 9) */
    snprintf(question, sizeof question, "«%s»:", word);
    for (int i = 0; i < n && i < 9; i++) {
        char item[300];

        snprintf(item, sizeof item, " %d %s ", i + 1, list[i]);
        if ((int)utf8_count(question, strlen(question)) + (int)utf8_count(item, strlen(item)) +
            34 > COLS)
            break;
        strcat(question, item);
        keys[nk++] = (char)('1' + i);
        shown++;
    }
    if (n == 0)
        strcat(question, " (sense suggeriments)");
    strcat(question, " · a afegir · i ignorar · Esc ");
    keys[nk++] = 'a';
    keys[nk++] = 'i';
    keys[nk] = '\0';

    c = prompt_choice(e, question, keys);
    if (c >= '1' && c < '1' + shown) {
        const char *rep = list[c - '1'];

        ed_select(e, a, z);
        ed_insert(e, rep, strlen(rep));
        ed_msg(e, "«%s» → «%s»", word, rep);
    } else if (c == 'a') {
        if (spell_add(word, len) < 0)
            ed_msg(e, "«%s» afegida, però no s'ha pogut desar el diccionari personal: %s",
                   word, strerror(errno));
        else
            ed_msg(e, "«%s» afegida al diccionari personal", word);
    } else if (c == 'i') {
        spell_ignore(word, len);
        ed_msg(e, "«%s» s'ignorarà fins que es tanqui Phileas", word);
    } else {
        e->msg[0] = '\0';
    }
    spell_free_list(list, n);
}

static void c_spell_toggle(Editor *e)
{
    char code[16];

    if (e->spell) {
        e->spell = 0;
        ed_msg(e, "Ortografia desactivada");
        return;
    }
    if (!need_dict(e))
        return;
    e->spell = 1;
    spell_code(spell_lang(), code, sizeof code);
    ed_msg(e, "Ortografia activada: %s (F8 canvia l'idioma · Maj+F7 error següent)", code);
}

/* Rotació d'idiomes amb F8 (opció «idiomes» de la configuració) */
static void c_rotate_lang(Editor *e)
{
    char code[16], next[16];
    int i;

    spell_code(e->dict, code, sizeof code);
    for (i = 0; i < e->nlangs; i++) {
        char c[16];

        spell_code(e->langs[i], c, sizeof c);
        if (!strcmp(code, c))
            break;
    }
    snprintf(next, sizeof next, "%s", e->langs[(i + 1) % e->nlangs]);
    if (e->nlangs == 1 && !strcmp(code, next)) {
        ed_msg(e, "Només hi ha un idioma configurat (%s): opció «idiomes»", next);
        return;
    }
    if (!set_dict(e, next))
        return;
    if (e->spell)
        ed_msg(e, "Idioma de l'ortografia: %s", next);
    else
        ed_msg(e, "Idioma de l'ortografia: %s (l'ortografia està desactivada: F7)", next);
}

static void c_language(Editor *e)
{
    char name[64];

    snprintf(name, sizeof name, "%s", e->dict);
    if (!prompt(e, "Diccionari (ca, es, ca_ES-valencia, en_US...): ", name, sizeof name, NULL) ||
        !name[0])
        return;
    e->spell = 1;
    if (!set_dict(e, name)) {
        e->spell = spell_lang() != NULL;
        return;
    }
    ed_msg(e, "Diccionari: %s", spell_lang());
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
    { "maquina-escriure",  c_typewriter, "Desplaçament: salt, línia fixa o clàssic", ACT_NONE },
    { "recarregar-configuracio", c_reload_config, "Torna a llegir el fitxer phileasrc", ACT_NONE },
    { "colors",            c_colors,     "Activa o desactiva els colors de Markdown", ACT_NONE },
    { "obrir",             c_open,       "Obre un document amb el navegador de fitxers", ACT_NONE },
    { "nou",               c_new,        "Comença un document nou sense nom", ACT_NONE },
    { "desar",             c_save,       "Desa el document", ACT_NONE },
    { "desar-com",         c_save_as,    "Desa el document amb un altre nom", ACT_NONE },
    { "sortir",            c_quit,       "Surt de Phileas", ACT_NONE },
    { "redibuixar",        c_redraw,     "Torna a dibuixar la pantalla", ACT_NONE },
    { "paleta",            c_palette,    "Executa una ordre pel nom", ACT_NONE },
    { "ajuda",             c_help,       "Mostra aquesta ajuda", ACT_NONE },

    { "desfer",            c_undo,       "Desfà l'últim canvi", ACT_NONE },
    { "refer",             c_redo,       "Refà el que s'ha desfet", ACT_NONE },
    { "cercar",            c_find,       "Cerca un text (mentre s'escriu)", ACT_NONE },
    { "cercar-seguent",    c_find_next,  "Següent coincidència", ACT_NONE },
    { "cercar-anterior",   c_find_prev,  "Coincidència anterior", ACT_NONE },
    { "substituir",        c_replace,    "Substitueix un text, amb confirmació", ACT_NONE },
    { "autodesat",         c_autosave,   "Activa o desactiva l'autodesat", ACT_NONE },
    { "revisar-ortografia", c_spell_next, "Selecciona l'error ortogràfic següent", ACT_NONE },
    { "suggeriments",      c_suggest,    "Suggeriments per a la paraula del cursor", ACT_NONE },
    { "ortografia",        c_spell_toggle, "Marca o no els errors ortogràfics", ACT_NONE },
    { "idioma",            c_language,   "Tria un diccionari pel nom", ACT_NONE },
    { "alternar-idioma",   c_rotate_lang, "Alterna el diccionari entre català i castellà", ACT_NONE },
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
    { "ctrl-shift-up", "seleccio-paragraf-anterior" },
    { "ctrl-shift-down", "seleccio-paragraf-seguent" },
    { "ctrl-shift-home", "seleccio-inici-document" },
    { "ctrl-shift-end", "seleccio-fi-document" },
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
    { "ctrl-o", "obrir" },            { "ctrl-n", "nou" },
    { "ctrl-s", "desar" },            { "alt-s", "desar-com" },
    { "ctrl-q", "sortir" },
    { "ctrl-l", "redibuixar" },
    { "ctrl-p", "paleta" },           { "f1", "ajuda" },

    { "ctrl-z", "desfer" },           { "ctrl-y", "refer" },
    { "ctrl-f", "cercar" },
    { "ctrl-g", "cercar-seguent" },   { "f3", "cercar-seguent" },
    { "alt-g", "cercar-anterior" },   { "shift-f3", "cercar-anterior" },
    { "ctrl-r", "substituir" },
    { "f7", "ortografia" },          { "shift-f7", "revisar-ortografia" },
    { "f8", "alternar-idioma" },     { "alt-enter", "suggeriments" },
};

#define MAX_BINDINGS 256

static struct {
    int key;
    const Command *cmd;
} bindings[MAX_BINDINGS];
static size_t nbindings;

#define NCOMMANDS (sizeof commands / sizeof commands[0])

/* ---- Ordres externes ---- */

/* El Command ha de ser el primer camp: cmd_run() passa d'un a l'altre */
typedef struct {
    Command cmd;
    char    name[32];
    char    desc[96];
    char   *shell;
    int     brief;
} External;

#define MAX_EXTERNALS 32
static External externals[MAX_EXTERNALS];
static size_t nexternals;

static External *as_external(const Command *c)
{
    if (c >= &externals[0].cmd && c < &externals[nexternals].cmd)
        return (External *)c;
    return NULL;
}

void cmd_extern_clear(void)
{
    for (size_t i = 0; i < nexternals; i++)
        free(externals[i].shell);
    nexternals = 0;
}

int cmd_config_extern(const char *name, const char *shell, int brief, char *err, size_t errsz)
{
    External *x = NULL;

    for (size_t i = 0; i < NCOMMANDS; i++) {
        if (!strcmp(commands[i].name, name)) {
            snprintf(err, errsz, "«%s» ja és una ordre de Phileas", name);
            return -1;
        }
    }
    for (size_t i = 0; i < nexternals && !x; i++)
        if (!strcmp(externals[i].name, name))
            x = &externals[i];
    if (!x) {
        if (nexternals == MAX_EXTERNALS) {
            snprintf(err, errsz, "massa ordres externes (com a molt %d)", MAX_EXTERNALS);
            return -1;
        }
        x = &externals[nexternals++];
        snprintf(x->name, sizeof x->name, "%s", name);
        x->shell = NULL;
    }
    free(x->shell);
    x->shell = xstrdup(shell);
    x->brief = brief;
    snprintf(x->desc, sizeof x->desc, "Ordre externa: %s", shell);
    x->cmd = (Command){ x->name, NULL, x->desc, ACT_NONE };
    return 0;
}

/* L'ordre de shell amb els arguments, cadascun entre cometes simples */
static char *extern_command(const External *x, const char *args)
{
    size_t cap = strlen(x->shell) + 4 * strlen(args ? args : "") + 16, n;
    char *s = xmalloc(cap);

    n = (size_t)snprintf(s, cap, "%s", x->shell);
    while (args && *args) {
        size_t len = strcspn(args, " ");

        s[n++] = ' ';
        s[n++] = '\'';
        for (size_t i = 0; i < len; i++) {
            if (args[i] == '\'') {
                memcpy(s + n, "'\\''", 4);
                n += 4;
            } else {
                s[n++] = args[i];
            }
        }
        s[n++] = '\'';
        args += len;
        args += strspn(args, " ");
    }
    s[n] = '\0';
    return s;
}

/* Missatge d'error segons l'estat de sortida (de system o pclose) */
static int extern_failed(Editor *e, const External *x, int st, const char *last)
{
    if (st != -1 && WIFEXITED(st) && WEXITSTATUS(st) == 0)
        return 0;
    if (st != -1 && WIFEXITED(st) && WEXITSTATUS(st) == 127)
        ed_msg(e, "%s: no s'ha trobat l'ordre «%s»", x->name, x->shell);
    else if (st != -1 && WIFEXITED(st))
        ed_msg(e, "%s: %s%s(error %d)", x->name, last ? last : "", last && *last ? " " : "",
               WEXITSTATUS(st));
    else
        ed_msg(e, "%s: no s'ha pogut executar", x->name);
    return 1;
}

/* Breu: sense teclat, i la darrera línia de la sortida a la barra d'estat */
static void run_brief(Editor *e, const External *x, const char *cmd)
{
    char *full = xmalloc(strlen(cmd) + 32);
    char line[256], last[256] = "";
    FILE *p;
    int st;

    ed_msg(e, "%s: executant…", x->name);
    render(e);
    refresh();
    sprintf(full, "%s </dev/null 2>&1", cmd);
    p = popen(full, "r");
    free(full);
    if (!p) {
        ed_msg(e, "%s: no s'ha pogut executar", x->name);
        return;
    }
    while (fgets(line, sizeof line, p)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0])
            snprintf(last, sizeof last, "%s", line);
    }
    st = pclose(p);
    /* La lletra o el gir de la consola en canvien la mida, i el que hi
     * hagi escrit l'ordre s'ha d'esborrar */
    ui_sync_size();
    clearok(curscr, TRUE);
    if (extern_failed(e, x, st, last))
        return;
    if (last[0])
        ed_msg(e, "%s", last);
    else
        ed_msg(e, "%s: fet", x->name);
}

/* Interactiva: el terminal en mode normal; Retorn torna a Phileas */
static void run_interactive(Editor *e, const External *x, const char *cmd)
{
    const char *clear = tigetstr("clear");
    char ch;
    int st;

    ui_suspend();
    if (clear && clear != (char *)-1)
        putp(clear);
    fflush(stdout);
    st = system(cmd);
    fputs("\nPrem Retorn per tornar a Phileas.", stdout);
    fflush(stdout);
    while (read(STDIN_FILENO, &ch, 1) == 1 && ch != '\n')
        ;
    ui_resume();
    if (!extern_failed(e, x, st, NULL))
        ed_msg(e, "%s: fet", x->name);
}

static void run_external(Editor *e, const External *x, const char *args)
{
    char *cmd;

    /* L'ordre pot llegir el document (exporta, envia): primer es desa */
    if (e->buf.modified && e->filename) {
        save_to(e, e->filename);
        if (e->buf.modified)
            return;
    }
    cmd = extern_command(x, args);
    if (x->brief)
        run_brief(e, x, cmd);
    else
        run_interactive(e, x, cmd);
    free(cmd);
}

void cmd_run(Editor *e, const Command *c, const char *args)
{
    const External *x = as_external(c);

    if (x)
        run_external(e, x, args);
    else if (args && *args)
        ed_msg(e, "L'ordre %s no admet arguments", c->name);
    else
        c->fn(e);
}

const Command *cmd_find(const char *name)
{
    for (size_t i = 0; i < NCOMMANDS; i++)
        if (!strcmp(commands[i].name, name))
            return &commands[i];
    for (size_t i = 0; i < nexternals; i++)
        if (!strcmp(externals[i].name, name))
            return &externals[i].cmd;
    return NULL;
}

size_t cmd_count(void)
{
    return NCOMMANDS + nexternals;
}

const Command *cmd_at(size_t i)
{
    return i < NCOMMANDS ? &commands[i] : &externals[i - NCOMMANDS].cmd;
}

int cmd_keys(const Command *c, int *keys, int max)
{
    int n = 0;

    for (size_t i = 0; i < nbindings; i++)
        if (bindings[i].cmd == c && n < max)
            keys[n++] = bindings[i].key;
    return n;
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

int cmd_unbind(const char *keyname)
{
    int key = key_parse(keyname);

    if (key < 0)
        return -1;
    for (size_t i = 0; i < nbindings; i++) {
        if (bindings[i].key == key) {
            bindings[i] = bindings[--nbindings];
            break;
        }
    }
    return 0;
}

int cmd_config_bind(const char *keyname, const char *cmdname)
{
    return *cmdname ? cmd_bind(keyname, cmdname) : cmd_unbind(keyname);
}

void cmd_bind_defaults(void)
{
    nbindings = 0;
    for (size_t i = 0; i < sizeof default_bindings / sizeof default_bindings[0]; i++) {
        if (!cmd_find(default_bindings[i][1]))
            die("drecera per defecte amb una ordre inexistent: %s", default_bindings[i][1]);
        if (cmd_bind(default_bindings[i][0], default_bindings[i][1]) < 0)
            die("drecera per defecte amb una tecla desconeguda: %s", default_bindings[i][0]);
    }
}
