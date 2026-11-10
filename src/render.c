#include "render.h"
#include "keys.h"
#include "md.h"
#include "spell.h"
#include "theme.h"
#include "todo.h"
#include "tui.h"
#include "utf8.h"
#include "wrap.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <wctype.h>

/* Parelles de colors */
enum { PAIR_H1 = 1, PAIR_H2, PAIR_H3, PAIR_CODE, PAIR_QUOTE, PAIR_MARK, PAIR_ERROR,
       PAIR_TEXT, PAIR_BAR, PAIR_SEL, PAIR_MENU, PAIR_DIM, PAIR_N };

static attr_t italic_attr = A_UNDERLINE;  /* si el terminal no té cursiva */
static attr_t mark_attr;
static attr_t no_color;   /* atributs que el terminal no combina amb colors (ncv) */
static int    have_colors;
static short  default_bg = -1;
static attr_t pair_bold[PAIR_N];  /* A_BOLD on el color clar no existeix (8 colors) */
static int    mark_colored;       /* els marcadors tenen color propi (si no, atenuats) */
static int    role_on[UI_N];      /* la barra, la selecció o els menús tenen color */
static const unsigned *console_rgb;  /* paleta del tema aplicada a la consola */
static attr_t focus_attr;            /* text atenuat de l'enfocament */
static long cursor_rgb = -1;         /* color del cursor posat pel tema, o -1 */

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
    }
    /* Marcadors atenuats; si el terminal no en sap, en blau */
    if (ta & A_DIM)
        mark_attr = A_DIM;
    else if (have_colors)
        mark_attr = COLOR_PAIR(PAIR_MARK);
}

static attr_t pair_attr(short pair)
{
    return COLOR_PAIR(pair) | pair_bold[pair];
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
        if (mark_colored || !(mark_attr & A_DIM))
            pair = PAIR_MARK;
        else
            a |= A_DIM;
    }
    /* A la consola de Linux, el subratllat i l'atenuat no van amb color:
     * mana l'atribut */
    return have_colors && pair && !(a & no_color) ? a | pair_attr(pair) : a;
}

attr_t ui_attr(int role)
{
    static const short pairs[UI_N] = { PAIR_BAR, PAIR_SEL, PAIR_MENU };

    if (have_colors && role_on[role])
        return pair_attr(pairs[role]);
    return role == UI_MENU ? A_NORMAL : A_REVERSE;
}

/* Un atribut amb el de la selecció (o un altre paper) a sobre: el color del
 * paper substitueix el de l'estil, i el vídeo invers s'hi afegeix. */
static attr_t over_role(attr_t a, int role)
{
    attr_t r = ui_attr(role);

    return r & A_COLOR ? (a & ~A_COLOR) | r : a | r;
}

/* La consola de Linux permet redefinir els seus 16 colors (ESC ] P) i
 * tornar als de sempre (ESC ] R). */
static int linux_console(void)
{
    const char *t = getenv("TERM");

    return t && !strncmp(t, "linux", 5);
}

static void console_palette(const unsigned *rgb)
{
    char seq[16 * 10 + 4];
    int n = 0;

    if (rgb)
        for (int i = 0; i < 16; i++)
            n += snprintf(seq + n, sizeof seq - (size_t)n, "\033]P%X%06X", i, rgb[i] & 0xFFFFFF);
    else
        n = snprintf(seq, sizeof seq, "\033]R");
    if (write(STDOUT_FILENO, seq, (size_t)n) < 0) {
        /* sense paleta pròpia: es veuen els colors normals */
    }
}

/* Fora de la consola, molts terminals (GNOME Terminal) pinten el cursor
 * amb un color fix del perfil, que pot ser el del fons d'un tema clar: el
 * tema hi posa el seu color de text (ESC ] 12) i en sortir es torna al del
 * terminal (ESC ] 112). La consola de Linux no ho entén ni ho necessita. */
static void cursor_color(long rgb)
{
    char seq[24];
    int n;

    if (rgb >= 0)
        n = snprintf(seq, sizeof seq, "\033]12;#%06lX\007", (unsigned long)rgb & 0xFFFFFF);
    else
        n = snprintf(seq, sizeof seq, "\033]112\007");
    if (write(STDOUT_FILENO, seq, (size_t)n) < 0) {
        /* sense color propi: el cursor del terminal */
    }
}

/* Parella amb els colors c (primer pla) i b (fons) de la paleta; PAL_DEFAULT
 * vol dir el del text. Els colors clars, si el terminal només en té 8, són
 * el normal amb negreta (a la consola de Linux, la negreta els aclareix). */
static void set_pair(short pair, short c, short b, short text_fg, short text_bg)
{
    pair_bold[pair] = 0;
    if (c == PAL_DEFAULT)
        c = text_fg;
    if (c >= 8 && c >= COLORS) {
        c -= 8;
        pair_bold[pair] = A_BOLD;
    }
    if (b == PAL_DEFAULT)
        b = text_bg;
    else if (b >= 8 && b >= COLORS)
        b -= 8;
    init_pair(pair, c, b);
}

void render_colors(Editor *e)
{
    static const short md_pairs[][2] = {
        { PAL_H1, PAIR_H1 }, { PAL_H2, PAIR_H2 }, { PAL_H3, PAIR_H3 },
        { PAL_CODE, PAIR_CODE }, { PAL_QUOTE, PAIR_QUOTE }, { PAL_ERROR, PAIR_ERROR },
    };
    static const short roles[UI_N][3] = {
        { PAL_BAR, PAL_BAR_BG, PAIR_BAR }, { PAL_SEL, PAL_SEL_BG, PAIR_SEL },
        { PAL_MENU, PAL_MENU_BG, PAIR_MENU },
    };
    const short *pal = e->palette;
    const unsigned *rgb;
    short text_fg, text_bg;
    long cur;
    int themed;

    if (!have_colors)
        return;
    /* «cap»: el color normal, si el terminal en té (si no, blanc sobre negre) */
    text_fg = pal[PAL_TEXT] != PAL_DEFAULT ? pal[PAL_TEXT] :
              default_bg != -1 ? COLOR_WHITE : -1;
    text_bg = pal[PAL_BG] != PAL_DEFAULT ? pal[PAL_BG] : default_bg;
    if (text_bg >= 8 && text_bg >= COLORS)
        text_bg -= 8;

    set_pair(PAIR_TEXT, text_fg, text_bg, text_fg, text_bg);
    themed = pal[PAL_TEXT] != PAL_DEFAULT || pal[PAL_BG] != PAL_DEFAULT;
    bkgd(' ' | (themed ? pair_attr(PAIR_TEXT) : 0));

    for (size_t i = 0; i < sizeof md_pairs / sizeof *md_pairs; i++)
        set_pair(md_pairs[i][1], pal[md_pairs[i][0]], PAL_DEFAULT, text_fg, text_bg);
    mark_colored = pal[PAL_MARK] != PAL_DEFAULT;
    set_pair(PAIR_MARK, mark_colored ? pal[PAL_MARK] : COLOR_BLUE, PAL_DEFAULT, text_fg, text_bg);
    for (int r = 0; r < UI_N; r++) {
        role_on[r] = pal[roles[r][0]] != PAL_DEFAULT || pal[roles[r][1]] != PAL_DEFAULT;
        set_pair(roles[r][2], pal[roles[r][0]], pal[roles[r][1]], text_fg, text_bg);
    }

    rgb = e->console_palette && linux_console() ? theme_at((size_t)e->theme)->rgb : NULL;
    if (rgb != console_rgb) {
        console_palette(rgb);
        console_rgb = rgb;
        clearok(curscr, TRUE);
    }

    /* Enfocament: l'atenuat del terminal si el pot combinar amb el color del
     * text; a la consola, el color atenuat del tema; si no, el dels marcadors */
    if (console_rgb && theme_at((size_t)e->theme)->dim >= 0) {
        set_pair(PAIR_DIM, theme_at((size_t)e->theme)->dim, PAL_DEFAULT, text_fg, text_bg);
        focus_attr = pair_attr(PAIR_DIM);
    } else if ((mark_attr & A_DIM) && (!(no_color & A_DIM) || !themed)) {
        focus_attr = A_DIM;
    } else {
        focus_attr = style_attr(MS_MARK);
    }

    cur = -1;
    rgb = theme_at((size_t)e->theme)->rgb;
    if (!linux_console() && rgb && pal[PAL_TEXT] >= 0 && pal[PAL_TEXT] < 16)
        cur = (long)rgb[pal[PAL_TEXT]];
    if (cur != cursor_rgb) {
        cursor_color(cur);
        cursor_rgb = cur;
    }
}

/* Error ortogràfic: vermell i subratllat (a la consola de Linux, només
 * vermell; sense colors, només subratllat) */
static attr_t error_attr(attr_t a)
{
    if (!have_colors)
        return a | A_UNDERLINE;
    a = (a & ~(A_COLOR | no_color)) | pair_attr(PAIR_ERROR);
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
    /* Pendents (TODO, [?], %%nota%%): errors[] = 2 */
    if (!e->buf.plain)
        for (from = 0; todo_next(l->text, l->len, l->kind, from, &a, &b); from = b)
            memset(errors + a, 2, b - a);
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
        if (console_rgb)
            console_palette(NULL);
        if (cursor_rgb >= 0)
            cursor_color(-1);
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
    if (console_rgb)
        console_palette(console_rgb);
    if (cursor_rgb >= 0)
        cursor_color(cursor_rgb);
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

/* Una línia que només conté "***" es mostra centrada a la pantalla (només
 * a la vista: el fitxer no canvia). Retorna la columna de pantalla on
 * comença, o -1 si la línia no es centra. */
static int centred_x(const Line *l, size_t r)
{
    if (r != 0 || l->nrows != 1 || l->len != 3 || memcmp(l->text, "***", 3) != 0)
        return -1;
    return COLS > 3 ? (COLS - 3) / 2 : 0;
}

/* Dibuixa la fila r de la línia y. [s0, s1) és la part seleccionada i, amb
 * l'enfocament, [f0, f1) la que no s'atenua. */
static void draw_row(Editor *e, Line *l, size_t r, int y, size_t s0, size_t s1,
                     size_t f0, size_t f1)
{
    size_t i = l->rows[r], end = wrap_row_end(l, r);
    int indent = wrap_row_indent(l, r);
    int cx = centred_x(l, r);
    int limit = cx >= 0 ? COLS - cx : e->width - indent;
    int col = 0;

    move(y, cx >= 0 ? cx : e->margin + indent);
    while (i < end) {
        wchar_t wc;
        size_t n = utf8_decode(l->text, l->len, i, &wc);
        int w = utf8_width(wc, col);
        int sel = i >= s0 && i < s1;

        if (col + w > limit)
            break;
        if (e->focus && (i < f0 || i >= f1)) {
            attr_t a = have_colors ? focus_attr : A_DIM;

            attrset(sel ? over_role(a, UI_SEL) : a);
        } else if (styles_cap) {
            attr_t a = e->colors ? style_attr(styles[i]) : 0;

            if (errors[i] == 2)
                a |= A_REVERSE | A_BOLD;
            else if (errors[i])
                a = error_attr(a);
            attrset(sel ? over_role(a, UI_SEL) : a);
        } else if (sel) {
            attrset(ui_attr(UI_SEL));
        }
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
        attrset(ui_attr(UI_SEL));
        addch(' ');
        attrset(A_NORMAL);
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
    char left[512], right[96], code[16] = "", font[40] = "";
    size_t words = buf_words(&e->buf);
    /* Mode de desplaçament, discret, al principi de la part dreta */
    const char *tw = e->tw_mode == TW_SALT ? "salt" : e->tw_mode == TW_FIXA ? "fixa" : "clàssic";
    int rlen;

    snprintf(left, sizeof left, " %s%s%s",
             e->filename ? e->filename : "[sense nom]",
             e->buf.modified ? " [+]" : "", e->buf.plain ? "  (text)" : "");
    if (e->spell && spell_lang()) {
        spell_code(spell_lang(), code, sizeof code - 2);
        strcat(code, "  ");
    }
    if (e->cstate.font[0])
        snprintf(font, sizeof font, "[%.*s]  ", (int)fit_chars(e->cstate.font, 20), e->cstate.font);
    snprintf(right, sizeof right, "%s%s  %s%zu %s  %zu pàg. ", font, tw, code,
             words, words == 1 ? "paraula" : "paraules", buf_pages(&e->buf));
    rlen = (int)utf8_count(right, strlen(right));

    attrset(ui_attr(UI_BAR));
    mvhline(y, 0, ' ', COLS);
    mvaddnstr(y, 0, left, (int)fit_chars(left, COLS - rlen - 1));
    if (rlen < COLS)
        mvaddstr(y, COLS - rlen, right);
    attrset(A_NORMAL);
}

void render(Editor *e)
{
    size_t tl, tr, crow, styled = (size_t)-1, fa = 0, fb = 0;
    int cur_y = -1, cur_x = 0, has_sel, cx;
    Pos sa, sz;

    ed_layout(e, COLS, LINES);
    buf_scan(&e->buf);   /* els colors depenen del tipus de bloc */
    erase();

    tl = e->top_line;
    tr = e->top_row;
    crow = ed_cursor_row(e);
    has_sel = ed_selection(e, &sa, &sz);
    if (e->focus)
        ed_focus_range(e, &fa, &fb);
    for (int y = e->top_blank; y < e->height; y++) {
        size_t s0 = 0, s1 = 0;

        if (has_sel && tl >= sa.line && tl <= sz.line) {
            s0 = tl == sa.line ? sa.col : 0;
            s1 = tl == sz.line ? sz.col : (size_t)-1;
        }
        if (tr < ed_line(e, tl)->nrows) {  /* si no, és la fila de separació */
            if (styled != tl)
                line_styles(e, styled = tl);
            draw_row(e, ed_line(e, tl), tr, y, s0, s1,
                     tl == e->cy ? fa : 0, tl == e->cy ? fb : 0);
        }
        if (tl == e->cy && tr == crow) {
            cur_y = y;
            cx = centred_x(ed_line(e, tl), tr);
            cur_x = (cx >= 0 ? cx : e->margin) + ed_cursor_x(e);
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
