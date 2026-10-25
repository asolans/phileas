#include "render.h"
#include "tui.h"
#include "utf8.h"
#include "wrap.h"

#include <stdio.h>
#include <string.h>

void ui_init(void)
{
    initscr();
    raw();               /* Ctrl+S, Ctrl+Q, Ctrl+C i Ctrl+Z arriben com a tecles */
    noecho();
    keypad(stdscr, TRUE);
    set_escdelay(25);
}

void ui_end(void)
{
    if (!isendwin())
        endwin();
}

static void draw_row(Editor *e, Line *l, size_t r, int y)
{
    size_t i = l->rows[r], end = wrap_row_end(l, r);
    int col = 0;

    move(y, e->margin);
    while (i < end) {
        wchar_t wc;
        size_t n = utf8_decode(l->text, l->len, i, &wc);
        int w = utf8_width(wc, col);

        if (col + w > e->width)
            break;
        if (wc == L'\t') {
            for (int k = 0; k < w; k++)
                addch(' ');
        } else if (!utf8_printable(wc)) {
            attron(A_REVERSE);
            addch('?');
            attroff(A_REVERSE);
        } else {
            addnwstr(&wc, 1);
        }
        col += w;
        i += n;
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
    char left[512], right[64];
    Line *l = &e->buf.lines[e->cy];
    int rlen;

    snprintf(left, sizeof left, " %s%s",
             e->filename ? e->filename : "[sense nom]",
             e->buf.modified ? " [+]" : "");
    snprintf(right, sizeof right, "L %zu/%zu  C %zu  ≈%zu pàg. ",
             e->cy + 1, e->buf.nlines, utf8_count(l->text, e->cx) + 1,
             buf_pages(&e->buf));
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
    size_t tl, tr, crow;
    int cur_y = -1, cur_x = 0;

    ed_layout(e, COLS, LINES);
    erase();

    tl = e->top_line;
    tr = e->top_row;
    crow = ed_cursor_row(e);
    for (int y = 0; y < e->height; y++) {
        draw_row(e, ed_line(e, tl), tr, y);
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

int prompt(Editor *e, const char *label, char *buf, size_t size)
{
    size_t len = strlen(buf);

    for (;;) {
        wint_t wc;
        int r;

        render(e);
        draw_prompt(label, buf);
        refresh();
        r = get_wch(&wc);
        if (r == ERR)
            continue;
        if (r == KEY_CODE_YES) {
            if (wc == KEY_ENTER)
                return 1;
            if (wc == KEY_BACKSPACE && len > 0)
                buf[len = utf8_prev(buf, len)] = '\0';
            continue;
        }
        if (wc == 27)
            return 0;
        if (wc == '\n' || wc == '\r')
            return 1;
        if (wc == 127 || wc == 8) {
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
    }
}

int prompt_yesno(Editor *e, const char *question)
{
    for (;;) {
        wint_t wc;
        int r;

        render(e);
        draw_prompt(question, " (s/n) ");
        refresh();
        r = get_wch(&wc);
        if (r == OK && (wc == 's' || wc == 'S'))
            return 1;
        if (r == OK && (wc == 'n' || wc == 'N' || wc == 27))
            return 0;
    }
}
