#include "search.h"
#include "utf8.h"

#include <string.h>
#include <wctype.h>

static int has_upper(const char *pat)
{
    size_t len = strlen(pat);

    for (size_t i = 0; i < len;) {
        wchar_t wc;
        i += utf8_decode(pat, len, i, &wc);
        if (wc != UTF8_INVALID && iswupper((wint_t)wc))
            return 1;
    }
    return 0;
}

/* Lletra sense accent (minúscula): à→a, ç→c, ñ→n… Les altres, tal qual. */
static wchar_t fold_accent(wchar_t c)
{
    static const struct { const wchar_t *from; wchar_t to; } map[] = {
        { L"àáâäãå", L'a' }, { L"çć", L'c' }, { L"èéêë", L'e' }, { L"ìíîï", L'i' },
        { L"ñ", L'n' },       { L"òóôöõ", L'o' }, { L"ùúûü", L'u' }, { L"ýÿ", L'y' },
    };

    for (size_t i = 0; i < sizeof map / sizeof *map; i++)
        for (const wchar_t *p = map[i].from; *p; p++)
            if (*p == c)
                return map[i].to;
    return c;
}

/* Si pat coincideix a s[i..], retorna on acaba; si no, (size_t)-1. */
static size_t match_at(const char *s, size_t len, size_t i,
                       const char *pat, size_t plen, int icase)
{
    size_t j = 0;

    while (j < plen) {
        wchar_t a, b;

        if (i >= len)
            return (size_t)-1;
        i += utf8_decode(s, len, i, &a);
        j += utf8_decode(pat, plen, j, &b);
        if (icase && a != UTF8_INVALID && b != UTF8_INVALID) {
            a = (wchar_t)towlower((wint_t)a);
            b = (wchar_t)towlower((wint_t)b);
            /* Una lletra sense accent al patró troba també la que en té
             * (cafe → cafè); si el patró hi posa l'accent, ha de coincidir */
            if (a != b && fold_accent(b) == b)
                a = fold_accent(a);
        }
        /* L'apòstrof recte i el tipogràfic són el mateix */
        if (a == L'’')
            a = L'\'';
        if (b == L'’')
            b = L'\'';
        if (a != b)
            return (size_t)-1;
    }
    return i;
}

/* Primera coincidència de la línia que comença a [from, to). */
static int first_in(const Line *l, size_t from, size_t to, const char *pat,
                    size_t plen, int icase, size_t *a, size_t *z)
{
    for (size_t i = from; i < to && i < l->len; i = utf8_next(l->text, l->len, i)) {
        size_t e = match_at(l->text, l->len, i, pat, plen, icase);
        if (e != (size_t)-1) {
            *a = i;
            *z = e;
            return 1;
        }
    }
    return 0;
}

/* Última coincidència de la línia que comença a [from, to). */
static int last_in(const Line *l, size_t from, size_t to, const char *pat,
                   size_t plen, int icase, size_t *a, size_t *z)
{
    int found = 0;
    size_t s, e;

    while (first_in(l, from, to, pat, plen, icase, &s, &e)) {
        *a = s;
        *z = e;
        found = 1;
        from = utf8_next(l->text, l->len, s);
    }
    return found;
}

int search_find(const Buffer *b, Pos from, const char *pat, int dir,
                Pos *a, Pos *z, int *wrapped)
{
    size_t plen = strlen(pat), n = b->nlines, s, e;
    int icase = !has_upper(pat);

    *wrapped = 0;
    if (plen == 0)
        return 0;

    if (dir > 0) {
        /* Resta de la línia actual, línies següents, i des del principi */
        for (size_t k = 0; k <= n; k++) {
            size_t y = (from.line + k) % n;
            size_t start = k == 0 ? from.col : 0;
            size_t stop = k == n ? from.col : (size_t)-1;

            if (from.line + k >= n && !*wrapped)
                *wrapped = 1;
            if (first_in(&b->lines[y], start, stop, pat, plen, icase, &s, &e)) {
                *a = (Pos){ y, s };
                *z = (Pos){ y, e };
                return 1;
            }
        }
    } else {
        for (size_t k = 0; k <= n; k++) {
            size_t y = (from.line + n - k % n) % n;
            size_t start = k == n ? from.col : 0;
            size_t stop = k == 0 ? from.col : (size_t)-1;

            if (k > from.line && !*wrapped)
                *wrapped = 1;
            if (last_in(&b->lines[y], start, stop, pat, plen, icase, &s, &e)) {
                *a = (Pos){ y, s };
                *z = (Pos){ y, e };
                return 1;
            }
        }
    }
    return 0;
}
