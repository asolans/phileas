#include "todo.h"
#include "md.h"
#include "util.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

int todo_next(const char *s, size_t len, int kind, size_t from, size_t *a, size_t *b)
{
    switch (MD_BASE(kind)) {
    case MD_CODE: case MD_FENCE_OPEN: case MD_FENCE_CLOSE: case MD_META: case MD_INDENT:
        return 0;
    }
    for (size_t i = 0; i < len; i++) {
        if (s[i] == '`') {
            /* Fragment de codi en línia: es salta, amb tants accents com n'obre */
            size_t n = 1, k;
            int found = 0;

            while (i + n < len && s[i + n] == '`')
                n++;
            for (k = i + n; k < len && !found;) {
                size_t m = 0;

                if (s[k] != '`') {
                    k++;
                    continue;
                }
                while (k + m < len && s[k + m] == '`')
                    m++;
                found = m == n;
                k += m;
            }
            i = found ? k - 1 : i + n - 1;
            continue;
        }
        if (s[i] == '\\') {
            i++;
            continue;
        }
        if (i < from)
            continue;
        if (s[i] == 'T' && i + 4 <= len && memcmp(s + i, "TODO", 4) == 0 &&
            (i == 0 || !isalnum((unsigned char)s[i - 1])) &&
            (i + 4 == len || !isalnum((unsigned char)s[i + 4]))) {
            *a = i;
            *b = i + 4;
            return 1;
        }
        if (s[i] == '[' && i + 3 <= len && s[i + 1] == '?' && s[i + 2] == ']') {
            *a = i;
            *b = i + 3;
            return 1;
        }
        if (s[i] == '%' && i + 1 < len && s[i + 1] == '%') {
            for (size_t k = i + 2; k + 1 < len; k++)
                if (s[k] == '%' && s[k + 1] == '%') {
                    *a = i;
                    *b = k + 2;
                    return 1;
                }
            i++;
        }
    }
    return 0;
}

size_t todo_collect(Buffer *b, size_t **lines)
{
    size_t n = 0, cap = 0;

    *lines = NULL;
    if (b->plain)
        return 0;
    buf_scan(b);
    for (size_t i = 0; i < b->nlines; i++) {
        size_t a, e;

        if (!todo_next(b->lines[i].text, b->lines[i].len, b->lines[i].kind, 0, &a, &e))
            continue;
        if (n == cap) {
            cap = cap ? cap * 2 : 16;
            *lines = xrealloc(*lines, cap * sizeof **lines);
        }
        (*lines)[n++] = i;
    }
    return n;
}
