#include "render.h"
#include "keys.h"
#include "md.h"
#include "spell.h"
#include "tui.h"
#include "utf8.h"
#include "wrap.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <wctype.h>

/* Parelles de colors (fons per defecte del terminal) */
enum { PAIR_H1 = 1, PAIR_H2, PAIR_H3, PAIR_CODE, PAIR_QUOTE, PAIR_MARK, PAIR_ERROR };

static attr_t italic_attr = A_UNDERLINE;  /* si el terminal no té cursiva */
static attr_t mark_attr;
static attr_t no_color;   /* atributs que el terminal no combina amb colors (ncv) */
static int    have_colors;
static short  default_bg = -1;

/* Estils de la línia que s'està dibuixant */
static unsigned char *styles, *errors;
static size_t styles_cap;

static void colors_init(void)
{
    attr_t ta = term_attrs();

    if (ta & A_ITALIC)
        italic_attr = A_ITALIC;
    if (has_colors() && start_color() == OK) {
        short bg = use_default_colors() == OK ? -1 : COLOR_BLACK;

        int ncv = tigetnum("ncv");

        have_colors = 1;
        if (ncv > 0)
            no_color = (ncv & 2 ? A_UNDERLINE : 0) | (ncv & 16 ? A_DIM : 0) |
                       (ncv & 0x8000 ? A_ITALIC : 0);
        default_bg = bg;
        init_pair(PAIR_MARK, COLOR_BLUE, bg);
    }
    /* Marcadors atenuats; si el terminal no en sap, en blau */
    if (ta & A_DIM)
        mark_attr = A_DIM;
    else if (have_colors)
        mark_attr = COLOR_PAIR(PAIR_MARK);
}

static attr_t style_attr(unsigned char st)
{
    static const short heading_pair[] = { 0, PAIR_H1, PAIR_H2, PAIR_H3 };
    attr_t a = 0;
    short pair = 0;

    if (MS_HLEVEL(st)) {
        a |= A_BOLD;
        pair = heading_pair[MS_HLEVEL(st)];
    }
    if (st & MS_QUOTE)
        pair = PAIR_QUOTE;
    if (st & MS_BOLD)
        a |= A_BOLD;
    if (st & MS_ITALIC)
        a |= italic_attr;
    if (st & MS_LINK)
        a |= A_UNDERLINE;
    if (st & MS_CODE)
        pair = PAIR_CODE;
    if (st & MS_MARK) {
        a &= ~A_BOLD;   /* negreta i atenuat es contradiuen */
        if (mark_attr & A_DIM)
            a |= A_DIM;
        else
            pair = PAIR_MARK;
    }
    /* A la consola de Linux, el subratllat i l'atenuat no van amb color:
     * mana l'atribut */
    return have_colors && pair && !(a & no_color) ? a | COLOR_PAIR(pair) : a;
}

void render_colors(Editor *e)
{
    static const short pairs[PAL_N] = {
        [PAL_H1] = PAIR_H1, [PAL_H2] = PAIR_H2, [PAL_H3] = PAIR_H3,
        [PAL_CODE] = PAIR_CODE, [PAL_QUOTE] = PAIR_QUOTE, [PAL_ERROR] = PAIR_ERROR,
    };

    if (!have_colors)
        return;
    for (int i = 0; i < PAL_N; i++) {
        short fg = e->palette[i];

        /* «cap»: el color normal, si el terminal en té (si no, blanc) */
        if (fg == PAL_DEFAULT && default_bg != -1)
            fg = COLOR_WHITE;
        init_pair(pairs[i], fg, default_bg);
    }
}

/* Error ortogràfic: vermell i subratllat (a la consola de Linux, només
 * vermell; sense colors, només subratllat) */
static attr_t error_attr(attr_t a)
{
    if (!have_colors)
        return a | A_UNDERLINE;
    a = (a & ~(A_COLOR | no_color)) | COLOR_PAIR(PAIR_ERROR);
    return no_color & A_UNDERLINE ? a : a | A_UNDERLINE;
}

/* Calcula els estils i els errors ortogràfics de la línia y. */
static void line_styles(Editor *e, size_t y)
{
    Line *l = ed_line(e, y);
    int setext = 0;
    size_t from = 0, a, b;

    if (l->len + 1 > styles_cap) {
        styles_cap = l->len + 64;
        free(styles);
        free(errors);
        styles = malloc(styles_cap);
        errors = malloc(styles_cap);
        if (!styles || !errors) {
            free(styles);
            free(errors);
            styles = errors = NULL;
            styles_cap = 0;
            return;
        }
    }
    if (MD_BASE(l->kind) == MD_TEXT && y + 1 < e->buf.nlines &&
        MD_BASE(e->buf.lines[y + 1].kind) == MD_UNDERLINE)
        setext = strchr(e->buf.lines[y + 1].text, '=') ? 1 : 2;
    if (e->buf.plain)
        memset(styles, 0, l->len);
    else
        md_style(l->text, l->len, l->kind, setext, styles);

    memset(errors, 0, l->len);
    while (e->spell && spell_next_error(l->text, l->len, styles, from, &a, &b)) {
        /* La paraula que s'està escrivint no es marca fins que s'acaba */
        if (!(y == e->cy && b == e->cx && e->last_act == ACT_TYPE))
            memset(errors + a, 1, b - a);
        from = b;
    }
}

void ui_init(void)
{
    initscr();
    raw();               /* Ctrl+S, Ctrl+Q, Ctrl+C i Ctrl+Z arriben com a tecles */
    noecho();
    keypad(stdscr, TRUE);
    set_escdelay(25);
    key_init();
    colors_init();
}

void ui_end(void)
{
    if (!isendwin()) {
        endwin();
        key_end();
    }
}

void ui_suspend(void)
{
    ui_end();
}

void ui_resume(void)
{
    reset_prog_mode();
    key_init();
    ui_sync_size();
    clearok(curscr, TRUE);
    refresh();
}

void ui_sync_size(void)
{
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_row > 0 && ws.ws_col > 0 &&
        (ws.ws_row != LINES || ws.ws_col != COLS))
        resize_term(ws.ws_row, ws.ws_col);
}

/* Dibuixa la fila r de la línia y. [s0, s1) és la part seleccionada. */
static void draw_row(Editor *e, Line *l, size_t r, int y, size_t s0, size_t s1)
{
    size_t i = l->rows[r], end = wrap_row_end(l, r);
    int indent = wrap_row_indent(l, r);
    int col = 0;

    move(y, e->margin + indent);
    while (i < end) {
        wchar_t wc;
        size_t n = utf8_decode(l->text, l->len, i, &wc);
        int w = utf8_width(wc, col);
        int sel = i >= s0 && i < s1;

        if (col + w > e->width - indent)
            break;
        if (styles_cap) {
            attr_t a = e->colors ? style_attr(styles[i]) : 0;
            attrset(errors[i] ? error_attr(a) : a);
        }
        if (sel)
            attron(A_REVERSE);
        if (wc == L'\t') {
            for (int k = 0; k < w; k++)
                addch(' ');
        } else if (!utf8_printable(wc)) {
            attrset(A_REVERSE);
            addch('?');
        } else {
            addnwstr(&wc, 1);
        }
        col += w;
        i += n;
    }
    attrset(A_NORMAL);
    /* El salt de línia seleccionat es mostra com un espai al final */
    if (end == l->len && s1 > l->len && col < e->width - indent) {
        attron(A_REVERSE);
        addch(' ');
        attroff(A_REVERSE);
    }
}

/* Bytes de s que caben en n caràcters. */
static size_t fit_chars(const char *s, int n)
{
    size_t len = strlen(s), i = 0;

    while (n-- > 0 && i < len)
        i = utf8_next(s, len, i);
    return i;
}

static void draw_status(Editor *e)
{
    int y = LINES - 2;
    char left[512], right[64], code[16] = "";
    size_t words = buf_words(&e->buf);
    int rlen;

    snprintf(left, sizeof left, " %s%s%s",
             e->filename ? e->filename : "[sense nom]",
             e->buf.modified ? " [+]" : "", e->buf.plain ? "  (text)" : "");
    if (e->spell && spell_lang()) {
        spell_code(spell_lang(), code, sizeof code - 2);
        strcat(code, "  ");
    }
    snprintf(right, sizeof right, "%s%zu %s  %zu pàg. ", code,
             words, words == 1 ? "paraula" : "paraules", buf_pages(&e->buf));
    rlen = (int)utf8_count(right, strlen(right));

    attron(A_REVERSE);
    mvhline(y, 0, ' ', COLS);
    mvaddnstr(y, 0, left, (int)fit_chars(left, COLS - rlen - 1));
    if (rlen < COLS)
        mvaddstr(y, COLS - rlen, right);
    attroff(A_REVERSE);
}

void render(Editor *e)
{
    size_t tl, tr, crow, styled = (size_t)-1;
    int cur_y = -1, cur_x = 0, has_sel;
    Pos sa, sz;

    ed_layout(e, COLS, LINES);
    buf_scan(&e->buf);   /* els colors depenen del tipus de bloc */
    erase();

    tl = e->top_line;
    tr = e->top_row;
    crow = ed_cursor_row(e);
    has_sel = ed_selection(e, &sa, &sz);
    for (int y = e->top_blank; y < e->height; y++) {
        size_t s0 = 0, s1 = 0;

        if (has_sel && tl >= sa.line && tl <= sz.line) {
            s0 = tl == sa.line ? sa.col : 0;
            s1 = tl == sz.line ? sz.col : (size_t)-1;
        }
        if (tr < ed_line(e, tl)->nrows) {  /* si no, és la fila de separació */
            if (styled != tl)
                line_styles(e, styled = tl);
            draw_row(e, ed_line(e, tl), tr, y, s0, s1);
        }
        if (tl == e->cy && tr == crow) {
            cur_y = y;
            cur_x = e->margin + ed_cursor_x(e);
        }
        if (!ed_vpos_next(e, &tl, &tr))
            break;
    }

    draw_status(e);
    mvaddnstr(LINES - 1, 0, e->msg, COLS);
    if (cur_y >= 0)
        move(cur_y, cur_x < COLS ? cur_x : COLS - 1);
}

static void draw_prompt(const char *label, const char *buf)
{
    move(LINES - 1, 0);
    clrtoeol();
    attron(A_BOLD);
    addnstr(label, COLS);
    attroff(A_BOLD);
    addstr(buf);
}

/* Text atenuat darrere de la resposta; el cursor es queda on era. */
static void draw_hint(const char *hint)
{
    int y, x;

    getyx(stdscr, y, x);
    if (!*hint || x >= COLS - 1)
        return;
    attron(A_DIM);
    addnstr(hint, (int)fit_chars(hint, COLS - x - 1));
    attroff(A_DIM);
    move(y, x);
}

static int prompt_ex(Editor *e, const char *label, char *buf, size_t size,
                     void (*changed)(Editor *e, const char *text),
                     void (*complete)(Editor *e, char *buf, size_t size,
                                      char *hint, size_t hintsz))
{
    size_t len = strlen(buf), old;
    char hint[512] = "";

    if (complete && !len)
        snprintf(hint, sizeof hint, "  (Tab completa el nom)");
    key_set_timeout(-1);
    for (;;) {
        wint_t wc;
        int r;

        render(e);
        draw_prompt(label, buf);
        draw_hint(hint);
        refresh();
        r = key_get_wch(&wc);
        if (r == ERR)
            continue;
        old = len;
        if (r == OK && wc == '\t') {
            if (complete) {
                complete(e, buf, size, hint, sizeof hint);
                len = strlen(buf);
            }
            continue;
        }
        hint[0] = '\0';
        if (r == KEY_CODE_YES) {
            if (wc == KEY_ENTER)
                return 1;
            if (wc == KEY_BACKSPACE && len > 0)
                buf[len = utf8_prev(buf, len)] = '\0';
        } else if (wc == 27) {
            return 0;
        } else if (wc == '\n' || wc == '\r') {
            return 1;
        } else if (wc == 127 || wc == 8) {
            if (len > 0)
                buf[len = utf8_prev(buf, len)] = '\0';
        } else if (utf8_printable((wchar_t)wc)) {
            char tmp[4];
            size_t n = utf8_encode((wchar_t)wc, tmp);

            if (len + n < size) {
                memcpy(buf + len, tmp, n);
                buf[len += n] = '\0';
            }
        }
        if (changed && len != old)
            changed(e, buf);
    }
}

int prompt(Editor *e, const char *label, char *buf, size_t size,
           void (*changed)(Editor *e, const char *text))
{
    return prompt_ex(e, label, buf, size, changed, NULL);
}

int prompt_complete(Editor *e, const char *label, char *buf, size_t size,
                    void (*complete)(Editor *e, char *buf, size_t size,
                                     char *hint, size_t hintsz))
{
    return prompt_ex(e, label, buf, size, NULL, complete);
}

int prompt_choice(Editor *e, const char *question, const char *choices)
{
    key_set_timeout(-1);
    for (;;) {
        wint_t wc;
        int r;

        render(e);
        draw_prompt(question, "");
        refresh();
        r = key_get_wch(&wc);
        if (r != OK)
            continue;
        if (wc == 27)
            return 0;
        if (wc < 128 && strchr(choices, towlower(wc)))
            return (int)towlower(wc);
    }
}

int prompt_yesno(Editor *e, const char *question)
{
    key_set_timeout(-1);
    for (;;) {
        wint_t wc;
        int r;

        render(e);
        draw_prompt(question, " (s/n) ");
        refresh();
        r = key_get_wch(&wc);
        if (r == OK && (wc == 's' || wc == 'S'))
            return 1;
        if (r == OK && (wc == 'n' || wc == 'N' || wc == 27))
            return 0;
    }
}
