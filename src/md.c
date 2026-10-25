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

static int all_chars(const char *s, size_t len, size_t i, char c)
{
    size_t n = 0;

    for (; i < len && s[i] == c; i++)
        n++;
    while (i < len && is_blank(s[i]))
        i++;
    return i == len ? (int)n : 0;
}

/* Longitud de la tanca ``` o ~~~ que comença a s[i] (0 si no n'és). */
static size_t fence_run(const char *s, size_t len, size_t i, char c)
{
    size_t n = 0;

    while (i + n < len && s[i + n] == c)
        n++;
    return n >= 3 ? n : 0;
}

int md_kind(const char *s, size_t len, MdState *st)
{
    int first = !st->started;
    size_t i = 0, n;
    int col = 0, k, hard;
    Prefix p;

    st->started = 1;
    while (i < len && i < 3 && s[i] == ' ')
        i++;

    if (st->fence_ch) {
        n = fence_run(s, len, i, (char)st->fence_ch);
        if (n >= st->fence_len && all_chars(s, len, i, (char)st->fence_ch)) {
            st->fence_ch = 0;
            return MD_FENCE_CLOSE;
        }
        return MD_CODE;
    }
    if (st->meta) {
        if ((len == 3 && !memcmp(s, "---", 3)) || (len == 3 && !memcmp(s, "...", 3)))
            st->meta = 0;
        return MD_META;
    }
    if (first && len == 3 && !memcmp(s, "---", 3)) {
        st->meta = 1;
        return MD_META;
    }

    for (size_t j = 0; j < len && is_blank(s[j]); j++)
        col += s[j] == '\t' ? 4 - col % 4 : 1;
    if (skip_blanks(s, len, 0) == len)
        return MD_EMPTY;
    hard = (len >= 2 && s[len - 1] == ' ' && s[len - 2] == ' ') || s[len - 1] == '\\';
    parse(s, len, &p);
    /* Un marcador de llista o de cita mana sobre el sagnat (llistes niades) */
    if (p.end > p.indent_end) {
        k = p.quote_end > p.indent_end ? MD_QUOTE : MD_LIST;
        return hard ? k | MD_HARDBREAK : k;
    }
    if (col >= 4)
        return MD_INDENT;

    if ((s[i] == '`' || s[i] == '~') && (n = fence_run(s, len, i, s[i]))) {
        st->fence_ch = (unsigned char)s[i];
        st->fence_len = (unsigned char)(n > 255 ? 255 : n);
        return MD_FENCE_OPEN;
    }

    if (s[i] == '#') {
        for (n = 0; i + n < len && s[i + n] == '#'; n++)
            ;
        if (n <= 6 && (i + n == len || is_blank(s[i + n])))
            return MD_HEADING;
    }
    if (s[i] == '|')
        return MD_TABLE;
    else if (s[i] == '<')
        return MD_HTML;
    else if (all_chars(s, len, i, '=') || all_chars(s, len, i, '-') >= 2)
        return MD_UNDERLINE;
    else
        k = MD_TEXT;
    return hard ? k | MD_HARDBREAK : k;
}

int md_tight(int k1, int k2)
{
    int b1 = MD_BASE(k1), b2 = MD_BASE(k2);

    if (b2 == MD_EMPTY)
        return 0;
    if (b1 == MD_FENCE_OPEN || b1 == MD_CODE)
        return 1;
    if (b1 == MD_META && b2 == MD_META)
        return 1;
    if (k1 & MD_HARDBREAK)
        return 1;
    if (b1 == b2 && (b1 == MD_LIST || b1 == MD_QUOTE || b1 == MD_TABLE ||
                     b1 == MD_HTML || b1 == MD_INDENT))
        return 1;
    /* Paràgraf sagnat dins d'un element de llista */
    if ((b1 == MD_LIST && b2 == MD_INDENT) || (b1 == MD_INDENT && b2 == MD_LIST))
        return 1;
    return b1 == MD_TEXT && b2 == MD_UNDERLINE;
}

int md_joinable(int k1, int k2)
{
    int b1 = MD_BASE(k1);

    return MD_BASE(k2) == MD_TEXT && !(k1 & MD_HARDBREAK) &&
           (b1 == MD_TEXT || b1 == MD_LIST || b1 == MD_QUOTE);
}
