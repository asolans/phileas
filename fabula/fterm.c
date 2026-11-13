#include "fterm.h"

#include "keys.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

static int           have_colors;
static short         default_bg = -1;
static attr_t        pair_bold[FT_N];
static attr_t        role_attr[FT_N];
static const unsigned *console_rgb;
static long          cursor_rgb = -1;
static const Theme  *cur_theme;
static int           want_console;
static short         cur_pal[PAL_N];
static int           have_pal;
static attr_t        no_color;      /* atributs que el terminal no combina amb color (ncv) */

static int linux_console(void)
{
    const char *t = getenv("TERM");

    return t && !strncmp(t, "linux", 5);
}

static void osc(const char *seq, int n)
{
    if (write(STDOUT_FILENO, seq, (size_t)n) < 0) {
        /* sense paleta o color propis: es veuen els normals */
    }
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
    osc(seq, n);
}

static void cursor_color(long rgb)
{
    char seq[24];
    int n;

    if (rgb >= 0)
        n = snprintf(seq, sizeof seq, "\033]12;#%06lX\007", (unsigned long)rgb & 0xFFFFFF);
    else
        n = snprintf(seq, sizeof seq, "\033]112\007");
    osc(seq, n);
}

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

static attr_t pair_attr(short pair)
{
    return COLOR_PAIR(pair) | pair_bold[pair];
}

/* Els mateixos colors que Phileas (render_colors): text i fons, barra,
 * selecció, menús, capçaleres, error i atenuat. L'atenuat, com el de
 * Phileas: el color dels marcadors si el tema en té; a la consola amb la
 * paleta del tema, el color atenuat del tema; l'atenuat del terminal si el
 * pot combinar amb el color; i si no, blau. */
static void setup_roles(void)
{
    short pal[PAL_N], text_fg, text_bg;
    const unsigned *rgb;
    long cur;
    int themed;

    for (int i = 0; i < FT_N; i++)
        role_attr[i] = 0;
    role_attr[FT_BAR] = role_attr[FT_SEL] = A_REVERSE;
    role_attr[FT_HEAD] = A_BOLD;
    role_attr[FT_DIM] = A_DIM;
    role_attr[FT_WARN] = A_BOLD;
    if (!have_colors || !cur_theme)
        return;
    if (have_pal)
        memcpy(pal, cur_pal, sizeof pal);
    else
        theme_palette(cur_theme, pal);
    text_fg = pal[PAL_TEXT] != PAL_DEFAULT ? pal[PAL_TEXT] : default_bg != -1 ? COLOR_WHITE : -1;
    text_bg = pal[PAL_BG] != PAL_DEFAULT ? pal[PAL_BG] : default_bg;
    if (text_bg >= 8 && text_bg >= COLORS)
        text_bg -= 8;

    /* la paleta de la consola primer: l'atenuat en depèn */
    rgb = want_console && linux_console() ? cur_theme->rgb : NULL;
    if (rgb != console_rgb) {
        console_palette(rgb);
        console_rgb = rgb;
        clearok(curscr, TRUE);
    }

    set_pair(FT_TEXT, text_fg, text_bg, text_fg, text_bg);
    themed = pal[PAL_TEXT] != PAL_DEFAULT || pal[PAL_BG] != PAL_DEFAULT;
    if (themed)
        role_attr[FT_TEXT] = pair_attr(FT_TEXT);
    bkgd(' ' | (themed ? pair_attr(FT_TEXT) : 0));
    if (pal[PAL_BAR] != PAL_DEFAULT || pal[PAL_BAR_BG] != PAL_DEFAULT) {
        set_pair(FT_BAR, pal[PAL_BAR], pal[PAL_BAR_BG], text_fg, text_bg);
        role_attr[FT_BAR] = pair_attr(FT_BAR);
    }
    if (pal[PAL_SEL] != PAL_DEFAULT || pal[PAL_SEL_BG] != PAL_DEFAULT) {
        set_pair(FT_SEL, pal[PAL_SEL], pal[PAL_SEL_BG], text_fg, text_bg);
        role_attr[FT_SEL] = pair_attr(FT_SEL);
    }
    if (pal[PAL_MENU] != PAL_DEFAULT || pal[PAL_MENU_BG] != PAL_DEFAULT) {
        set_pair(FT_MENU, pal[PAL_MENU], pal[PAL_MENU_BG], text_fg, text_bg);
        role_attr[FT_MENU] = pair_attr(FT_MENU);
    }
    if (pal[PAL_H1] != PAL_DEFAULT) {
        set_pair(FT_HEAD, pal[PAL_H1], PAL_DEFAULT, text_fg, text_bg);
        role_attr[FT_HEAD] = pair_attr(FT_HEAD) | A_BOLD;
    }
    if (pal[PAL_MARK] != PAL_DEFAULT) {
        set_pair(FT_DIM, pal[PAL_MARK], PAL_DEFAULT, text_fg, text_bg);
        role_attr[FT_DIM] = pair_attr(FT_DIM);
    } else if (console_rgb && cur_theme->dim >= 0) {
        set_pair(FT_DIM, cur_theme->dim, PAL_DEFAULT, text_fg, text_bg);
        role_attr[FT_DIM] = pair_attr(FT_DIM);
    } else if ((termattrs() & A_DIM) && (!(no_color & A_DIM) || !themed)) {
        role_attr[FT_DIM] = A_DIM;
    } else {
        set_pair(FT_DIM, COLOR_BLUE, PAL_DEFAULT, text_fg, text_bg);
        role_attr[FT_DIM] = pair_attr(FT_DIM);
    }
    if (pal[PAL_ERROR] != PAL_DEFAULT) {
        set_pair(FT_WARN, pal[PAL_ERROR], PAL_DEFAULT, text_fg, text_bg);
        role_attr[FT_WARN] = pair_attr(FT_WARN) | A_BOLD;
    }

    cur = -1;
    rgb = cur_theme->rgb;
    if (!linux_console() && rgb && pal[PAL_TEXT] >= 0 && pal[PAL_TEXT] < 16)
        cur = (long)rgb[pal[PAL_TEXT]];
    if (cur != cursor_rgb) {
        cursor_color(cur);
        cursor_rgb = cur;
    }
}

void ft_init(const Theme *t, int console_pal)
{
    ft_init_pal(t, console_pal, NULL);
}

void ft_init_pal(const Theme *t, int console_pal, const short *pal)
{
    cur_theme = t;
    want_console = console_pal;
    have_pal = pal != NULL;
    if (pal)
        memcpy(cur_pal, pal, sizeof cur_pal);
    initscr();
    raw();
    noecho();
    keypad(stdscr, TRUE);
    set_escdelay(25);
    curs_set(0);
    key_init();
    if (has_colors() && start_color() == OK) {
        int ncv = tigetnum("ncv");

        default_bg = use_default_colors() == OK ? -1 : COLOR_BLACK;
        have_colors = 1;
        if (ncv > 0)
            no_color = (ncv & 2 ? A_UNDERLINE : 0) | (ncv & 16 ? A_DIM : 0);
    }
    setup_roles();
}

void ft_end(void)
{
    if (!isendwin()) {
        curs_set(1);
        endwin();
        key_end();
        if (console_rgb)
            console_palette(NULL);
        if (cursor_rgb >= 0)
            cursor_color(-1);
    }
}

void ft_suspend(void)
{
    ft_end();
}

void ft_resume(void)
{
    reset_prog_mode();
    key_init();
    if (console_rgb)
        console_palette(console_rgb);
    if (cursor_rgb >= 0)
        cursor_color(cursor_rgb);
    ft_sync_size();
    curs_set(0);
    clearok(curscr, TRUE);
    refresh();
}

attr_t ft_attr(int role)
{
    return role >= 0 && role < FT_N ? role_attr[role] : 0;
}

void ft_sync_size(void)
{
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_row > 0 && ws.ws_col > 0 &&
        (ws.ws_row != LINES || ws.ws_col != COLS))
        resize_term(ws.ws_row, ws.ws_col);
}
