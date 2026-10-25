#include "wrap.h"
#include "utf8.h"
#include "util.h"

static void push_row(Line *l, size_t off)
{
    if (l->nrows == l->rows_cap) {
        l->rows_cap = l->rows_cap ? l->rows_cap * 2 : 4;
        l->rows = xrealloc(l->rows, l->rows_cap * sizeof *l->rows);
    }
    l->rows[l->nrows++] = off;
}

void wrap_line(Line *l, int width)
{
    size_t i = 0, start = 0, brk = 0;  /* brk: últim tall possible (després d'un espai) */
    int col = 0;

    if (l->wrap_w == width)
        return;
    l->nrows = 0;
    push_row(l, 0);
    while (i < l->len) {
        wchar_t wc;
        size_t n = utf8_decode(l->text, l->len, i, &wc);
        int w = utf8_width(wc, col);

        if (col > 0 && col + w > width) {
            if (wc == L' ') {
                while (i < l->len && l->text[i] == ' ')
                    i++;
                if (i == l->len)
                    break;
                start = i;
            } else {
                start = brk > start ? brk : i;
            }
            push_row(l, start);
            i = brk = start;
            col = 0;
            continue;
        }
        col += w;
        i += n;
        if (wc == L' ')
            brk = i;
    }
    l->wrap_w = width;
}

size_t wrap_row_of(const Line *l, size_t col)
{
    size_t r = l->nrows - 1;

    while (r > 0 && l->rows[r] > col)
        r--;
    return r;
}

size_t wrap_row_end(const Line *l, size_t r)
{
    return r + 1 < l->nrows ? l->rows[r + 1] : l->len;
}
