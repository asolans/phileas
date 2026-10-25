#include "utf8.h"

static int is_cont(unsigned char c)
{
    return (c & 0xC0) == 0x80;
}

size_t utf8_decode(const char *s, size_t len, size_t i, wchar_t *wc)
{
    const unsigned char *p = (const unsigned char *)s + i;
    size_t avail = len - i;
    unsigned char c = p[0];
    size_t n;
    wchar_t v;

    if (c < 0x80) {
        *wc = c;
        return 1;
    }
    if (c >= 0xC2 && c <= 0xDF) {
        n = 2;
        v = c & 0x1F;
    } else if (c >= 0xE0 && c <= 0xEF) {
        n = 3;
        v = c & 0x0F;
    } else if (c >= 0xF0 && c <= 0xF4) {
        n = 4;
        v = c & 0x07;
    } else {
        goto invalid;
    }
    if (avail < n)
        goto invalid;
    for (size_t k = 1; k < n; k++) {
        if (!is_cont(p[k]))
            goto invalid;
        v = (v << 6) | (p[k] & 0x3F);
    }
    /* Formes sobrellargues, suplents i valors més enllà de U+10FFFF */
    if ((n == 3 && v < 0x800) || (n == 4 && v < 0x10000) ||
        (v >= 0xD800 && v <= 0xDFFF) || v > 0x10FFFF)
        goto invalid;
    *wc = v;
    return n;

invalid:
    *wc = UTF8_INVALID;
    return 1;
}

size_t utf8_encode(wchar_t wc, char out[4])
{
    unsigned long v = (unsigned long)wc;

    if (v < 0x80) {
        out[0] = (char)v;
        return 1;
    }
    if (v < 0x800) {
        out[0] = (char)(0xC0 | (v >> 6));
        out[1] = (char)(0x80 | (v & 0x3F));
        return 2;
    }
    if (v < 0x10000) {
        out[0] = (char)(0xE0 | (v >> 12));
        out[1] = (char)(0x80 | ((v >> 6) & 0x3F));
        out[2] = (char)(0x80 | (v & 0x3F));
        return 3;
    }
    out[0] = (char)(0xF0 | (v >> 18));
    out[1] = (char)(0x80 | ((v >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((v >> 6) & 0x3F));
    out[3] = (char)(0x80 | (v & 0x3F));
    return 4;
}

size_t utf8_next(const char *s, size_t len, size_t i)
{
    wchar_t wc;

    return i < len ? i + utf8_decode(s, len, i, &wc) : len;
}

size_t utf8_prev(const char *s, size_t i)
{
    size_t j;
    wchar_t wc;

    if (i == 0)
        return 0;
    j = i - 1;
    while (j > 0 && i - j < 4 && is_cont((unsigned char)s[j]))
        j--;
    /* Només acceptem el salt si hi ha un caràcter vàlid que acaba just a i */
    if (utf8_decode(s, i, j, &wc) == i - j)
        return j;
    return i - 1;
}

static int is_combining(const char *s, size_t len, size_t i)
{
    wchar_t wc;

    utf8_decode(s, len, i, &wc);
    return wc != UTF8_INVALID && wc >= 0x300 && wcwidth(wc) == 0;
}

size_t utf8_next_cluster(const char *s, size_t len, size_t i)
{
    i = utf8_next(s, len, i);
    while (i < len && is_combining(s, len, i))
        i = utf8_next(s, len, i);
    return i;
}

size_t utf8_prev_cluster(const char *s, size_t i)
{
    size_t j = utf8_prev(s, i);

    while (j > 0 && is_combining(s, i, j))
        j = utf8_prev(s, j);
    return j;
}

size_t utf8_count(const char *s, size_t end)
{
    size_t n = 0;

    for (size_t i = 0; i < end; i = utf8_next(s, end, i))
        n++;
    return n;
}

int utf8_printable(wchar_t wc)
{
    if (wc == UTF8_INVALID || wc < 0x20 || (wc >= 0x7F && wc < 0xA0))
        return 0;
    return wcwidth(wc) >= 0;
}

int utf8_width(wchar_t wc, int col)
{
    if (wc == L'\t')
        return TAB_WIDTH - col % TAB_WIDTH;
    if (!utf8_printable(wc))
        return 1;
    return wcwidth(wc);
}
