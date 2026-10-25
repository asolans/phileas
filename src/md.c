#include "md.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    size_t indent_end;  /* final del sagnat inicial */
    size_t quote_end;   /* final del sagnat i les cites */
    size_t num_start;   /* número de llista ordenada (num_start == num_end si no n'hi ha) */
    size_t num_end;
    size_t marker_end;  /* final del marcador de llista i els espais */
    size_t end;         /* final de tot el prefix (inclou la casella) */
    int task;
} Prefix;

static int is_blank(char c)
{
    return c == ' ' || c == '\t';
}

static size_t skip_blanks(const char *s, size_t len, size_t i)
{
    while (i < len && is_blank(s[i]))
        i++;
    return i;
}

static void parse(const char *s, size_t len, Prefix *p)
{
    size_t i = skip_blanks(s, len, 0), j;

    memset(p, 0, sizeof *p);
    p->indent_end = i;
    while (i < len && s[i] == '>')
        i = skip_blanks(s, len, i + 1);
    p->quote_end = p->num_start = p->num_end = p->marker_end = p->end = i;

    if (i + 1 < len && strchr("-*+", s[i]) && is_blank(s[i + 1])) {
        j = i + 1;
    } else {
        for (j = i; j < len && j - i < 9 && s[j] >= '0' && s[j] <= '9'; j++)
            ;
        if (j == i || j + 1 >= len || (s[j] != '.' && s[j] != ')') || !is_blank(s[j + 1]))
            return;
        p->num_end = j;
        j++;
    }
    p->marker_end = p->end = skip_blanks(s, len, j);

    i = p->end;
    if (i + 3 < len && s[i] == '[' && strchr(" xX", s[i + 1]) && s[i + 2] == ']' &&
        is_blank(s[i + 3])) {
        p->task = 1;
        p->end = skip_blanks(s, len, i + 3);
    }
}

size_t md_prefix(const char *s, size_t len)
{
    Prefix p;

    parse(s, len, &p);
    return p.end;
}

size_t md_marker(const char *s, size_t len)
{
    Prefix p;

    parse(s, len, &p);
    return p.end > p.indent_end ? p.end : 0;
}

static size_t put(char *out, size_t outsz, size_t k, const char *s, size_t n)
{
    if (k + n >= outsz)
        n = outsz - 1 - k;
    memcpy(out + k, s, n);
    return k + n;
}

size_t md_continue(const char *s, size_t len, char *out, size_t outsz)
{
    Prefix p;
    size_t k = 0;

    if (outsz == 0)
        return 0;
    parse(s, len, &p);
    k = put(out, outsz, k, s, p.num_start);
    if (p.num_end > p.num_start) {
        char num[16];
        unsigned long n = 0;
        int w;

        for (size_t i = p.num_start; i < p.num_end; i++)
            n = n * 10 + (unsigned long)(s[i] - '0');
        w = snprintf(num, sizeof num, "%lu", n + 1);
        k = put(out, outsz, k, num, (size_t)w);
    }
    k = put(out, outsz, k, s + p.num_end, p.marker_end - p.num_end);
    if (p.task)
        k = put(out, outsz, k, "[ ] ", 4);
    out[k] = '\0';
    return k;
}
