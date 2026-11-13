#include "ui.h"

#include "keys.h"
#include "utf8.h"

#include <stdio.h>
#include <string.h>

void cm_look_start(const CmLook *l)
{
    const Theme *th = theme_find(l->tema);
    short pal[PAL_N];

    if (!th)
        th = theme_at(0);
    theme_palette(th, pal);
    cm_rc_apply(pal, l->over);
    ft_init_pal(th, l->console, pal);
}

int cm_width(const char *s)
{
    size_t n = strlen(s), i = 0;
    int w = 0;

    while (i < n) {
        wchar_t c;
        size_t k = utf8_decode(s, n, i, &c);
        int cw = c == UTF8_INVALID ? 1 : utf8_width(c, w);

        w += cw < 0 ? 1 : cw;
        i += k ? k : 1;
    }
    return w;
}

int cm_put(int y, int x, const char *s, int maxw, attr_t a)
{
    size_t n = strlen(s), i = 0, end = 0;
    int w = 0, wend = 0;

    if (maxw <= 0)
        return 0;
    while (i < n) {
        wchar_t c;
        size_t k = utf8_decode(s, n, i, &c);
        int cw = c == UTF8_INVALID ? 1 : utf8_width(c, w);

        if (cw < 0)
            cw = 1;
        if (w + cw > maxw)
            break;
        w += cw;
        i += k ? k : 1;
        if (w <= maxw - 1) {
            end = i;
            wend = w;
        }
    }
    attron(a);
    if (i >= n) {
        mvaddnstr(y, x, s, (int)n);
    } else {
        mvaddnstr(y, x, s, (int)end);
        mvaddstr(y, x + wend, "…");
        w = wend + 1;
    }
    attroff(a);
    return w;
}

void cm_blank(int y, int x, int w, attr_t a)
{
    attron(a);
    move(y, x);
    for (int i = 0; i < w; i++)
        addch(' ');
    attroff(a);
}

/* La barra d'estat amb el text s (la pantalla de sota, repintada si cal) */
static void bar(const char *s, cm_paint_fn paint, void *ud)
{
    if (paint)
        paint(ud);
    cm_blank(LINES - 1, 0, COLS, ft_attr(FT_BAR));
    cm_put(LINES - 1, 0, s, COLS, ft_attr(FT_BAR));
}

int cm_prompt(const char *label, const char *initial, char *buf, size_t sz,
              cm_paint_fn paint, void *ud)
{
    size_t l;

    snprintf(buf, sz, "%s", initial ? initial : "");
    l = strlen(buf);
    for (;;) {
        char shown[600];
        int col, key;

        snprintf(shown, sizeof shown, " %s%s", label, buf);
        bar(shown, paint, ud);
        col = cm_width(shown);
        curs_set(1);
        move(LINES - 1, col < COLS ? col : COLS - 1);
        refresh();
        key = key_read();
        if (key < 0 || key == K_FN(KEY_RESIZE))
            continue;
        if (key == '\n') {
            curs_set(0);
            return 1;
        } else if (key == 27) {
            curs_set(0);
            return 0;
        } else if (key == 21) {                         /* Ctrl+U */
            buf[l = 0] = '\0';
        } else if (key == K_FN(KEY_BACKSPACE) || key == 127 || key == 8) {
            while (l && ((unsigned char)buf[l - 1] & 0xC0) == 0x80)
                l--;
            if (l)
                buf[--l] = '\0';
        } else if (key >= 32 && !(key & (KEY_FLAG_FN | KEY_FLAG_ALT))) {
            char enc[4];
            size_t k = utf8_encode((wchar_t)key, enc);

            if (l + k < sz) {
                memcpy(buf + l, enc, k);
                buf[l += k] = '\0';
            }
        }
    }
}

int cm_confirm(const char *question, cm_paint_fn paint, void *ud)
{
    char shown[700];
    int key;

    snprintf(shown, sizeof shown, " %s (s/n) ", question);
    bar(shown, paint, ud);
    refresh();
    do
        key = key_read();
    while (key < 0 || key == K_FN(KEY_RESIZE));
    return key == 's' || key == 'S' || key == 'y' || key == 'Y';
}

int cm_menu(const CmMenuItem *items, int n, cm_paint_fn paint, void *ud)
{
    int sel = 1, w = 0, top = 0;

    for (int i = 0; i < n; i++) {
        int lw = cm_width(items[i].label) + 2 + (items[i].hint ? cm_width(items[i].hint) : 0) + 4;

        if (lw > w)
            w = lw;
    }
    if (w > COLS - 2)
        w = COLS - 2;
    for (;;) {
        int x0 = 1, y0 = 2, key, rows = LINES - y0 - 3, dir = 0;

        if (rows > n)
            rows = n;
        if (rows < 3)
            rows = 3;
        /* que la selecció (i la capçalera de damunt) sigui visible */
        if (sel - 1 < top && sel > 0 && !items[sel - 1].key)
            top = sel - 1;
        if (sel < top)
            top = sel;
        if (sel >= top + rows)
            top = sel - rows + 1;
        if (top + rows > n)
            top = n - rows;
        if (top < 0)
            top = 0;
        if (paint)
            paint(ud);
        attrset(A_NORMAL);
        attron(ft_attr(FT_DIM));
        mvaddstr(y0, x0, "┌");
        mvaddstr(y0 + 1 + rows, x0, "└");
        for (int c = 0; c < w; c++) {
            mvaddstr(y0, x0 + 1 + c, "─");
            mvaddstr(y0 + 1 + rows, x0 + 1 + c, "─");
        }
        mvaddstr(y0, x0 + w + 1, "┐");
        mvaddstr(y0 + 1 + rows, x0 + w + 1, "┘");
        attroff(ft_attr(FT_DIM));
        for (int r = 0; r < rows && top + r < n; r++) {
            int i = top + r, y = y0 + 1 + r;
            attr_t a = i == sel ? ft_attr(FT_SEL) : 0;

            attron(ft_attr(FT_DIM));
            mvaddstr(y, x0, "│");
            mvaddstr(y, x0 + w + 1, "│");
            attroff(ft_attr(FT_DIM));
            cm_blank(y, x0 + 1, w, a);
            if (!items[i].key) {
                cm_put(y, x0 + 2, items[i].label, w - 2, ft_attr(FT_HEAD));
            } else {
                int hw = cm_width(items[i].hint);

                cm_put(y, x0 + 4, items[i].label, w - 4, a);
                cm_put(y, x0 + w - hw - 1, items[i].hint, hw, a ? a : ft_attr(FT_DIM));
            }
        }
        if (top > 0)
            cm_put(y0, x0 + w - 2, "▲", 1, ft_attr(FT_DIM));
        if (top + rows < n)
            cm_put(y0 + 1 + rows, x0 + w - 2, "▼", 1, ft_attr(FT_DIM));
        refresh();
        key = key_read();
        if (key < 0 || key == K_FN(KEY_RESIZE))
            continue;
        if (key == 27 || key == K_FN(KEY_F(10)))
            return -1;
        if (key == K_FN(KEY_UP))
            dir = -1;
        else if (key == K_FN(KEY_DOWN))
            dir = 1;
        else if (key == K_FN(KEY_HOME)) {
            sel = 0;
            dir = 1;
        } else if (key == K_FN(KEY_END)) {
            sel = n;
            dir = -1;
        } else if (key == '\n' && items[sel].key)
            return items[sel].key;
        if (dir) {          /* salta les capçaleres, amb tornada */
            do
                sel = (sel + dir + n) % n;
            while (!items[sel].key);
        }
    }
}

void cm_help(const char *const *lines, size_t n)
{
    size_t top = 0;

    for (;;) {
        size_t rows = LINES > 1 ? (size_t)LINES : 1, max = n > rows ? n - rows : 0;
        int key;

        if (top > max)
            top = max;
        erase();
        for (size_t r = 0; r < rows && top + r < n; r++)
            cm_put((int)r, 1, lines[top + r], COLS - 2, top + r == 0 ? ft_attr(FT_HEAD) : 0);
        if (max) {                       /* no hi cap: indicació de desplaçament */
            const char *more = top < max ? (top ? " ▲▼ " : " ▼ ") : " ▲ ";

            cm_put(LINES - 1, COLS - cm_width(more) - 1, more, 4, ft_attr(FT_DIM));
        }
        refresh();
        key = key_read();
        if (key < 0 || key == K_FN(KEY_RESIZE))
            continue;
        if (max && key == K_FN(KEY_DOWN))
            top++;
        else if (max && key == K_FN(KEY_UP))
            top = top ? top - 1 : 0;
        else if (max && (key == K_FN(KEY_NPAGE) || key == ' '))
            top += rows - 1;
        else if (max && key == K_FN(KEY_PPAGE))
            top = top > rows - 1 ? top - (rows - 1) : 0;
        else
            return;
    }
}
