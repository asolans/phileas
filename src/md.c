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
    /* El salt dur només uneix el text que continua el paràgraf; una llista,
     * una cita o una capçalera comencen bloc igualment (si no, pujarien i
     * baixarien mentre s'escriuen els dos espais) */
    if ((k1 & MD_HARDBREAK) && (b2 == MD_TEXT || b2 == MD_INDENT))
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

/* ---- Estils ---- */

#define NONE ((size_t)-1)

static int is_word(unsigned char c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           c >= 0x80;
}

static int is_punct(unsigned char c)
{
    return c > ' ' && c < 0x7F && !is_word(c);
}

static size_t run(const char *s, size_t b, size_t i)
{
    size_t j = i;

    while (j < b && s[j] == s[i])
        j++;
    return j - i;
}

static void fill(unsigned char *out, size_t a, size_t b, unsigned char v)
{
    memset(out + a, v, b - a);
}

/* Inici de la tanca d'un fragment de codi de n accents oberts que comença
 * a from: una tira d'exactament n. NONE si no n'hi ha. */
static size_t code_close(const char *s, size_t b, size_t from, size_t n)
{
    for (size_t j = from; j < b;) {
        size_t r;

        if (s[j] != '`') {
            j++;
            continue;
        }
        r = run(s, b, j);
        if (r == n)
            return j;
        j += r;
    }
    return NONE;
}

/* Tanca d'un èmfasi de n caràcters c: una tira d'exactament n que no
 * segueix un espai (ni va davant d'una lletra, si és '_'). Se salten els
 * escapaments i els fragments de codi. */
static size_t emph_close(const char *s, size_t b, size_t from, char c, size_t n)
{
    for (size_t j = from; j < b;) {
        size_t r, k;

        if (s[j] == '\\') {
            j += 2;
        } else if (s[j] == '`') {
            r = run(s, b, j);
            k = code_close(s, b, j + r, r);
            j = k == NONE ? j + r : k + r;
        } else if (s[j] == c) {
            r = run(s, b, j);
            if (r == n && j > from && !is_blank(s[j - 1]) &&
                (c != '_' || j + r >= b || !is_word((unsigned char)s[j + r])))
                return j;
            j += r;
        } else {
            j++;
        }
    }
    return NONE;
}

/* Final (']') del text d'un enllaç que comença a from, i ')' de l'adreça.
 * Retorna 0 si no és un enllaç complet. */
static int link_end(const char *s, size_t b, size_t from, size_t *text_end, size_t *url_end)
{
    size_t j = from;

    while (j < b && s[j] != ']') {
        if (s[j] == '\\')
            j++;
        j++;
    }
    if (j + 1 >= b || s[j + 1] != '(')
        return 0;
    *text_end = j;
    for (j += 2; j < b && s[j] != ')'; j++)
        ;
    if (j >= b)
        return 0;
    *url_end = j;
    return 1;
}

size_t md_footnote(const char *s, size_t len, size_t i)
{
    size_t j = i + 2;

    if (i + 3 >= len || s[i] != '[' || s[i + 1] != '^')
        return 0;
    while (j < len && s[j] != ']' && s[j] != '[' && !is_blank(s[j]))
        j++;
    return j < len && s[j] == ']' && j > i + 2 ? j + 1 : 0;
}

size_t md_footnote_def(const char *s, size_t len)
{
    size_t i = 0, e;

    while (i < len && i < 3 && s[i] == ' ')
        i++;
    e = md_footnote(s, len, i);
    return e && e < len && s[e] == ':' ? i : NONE;
}

size_t md_next_footnote(const char *s, size_t len, size_t from)
{
    for (size_t j = from; j < len;) {
        size_t r, k;

        if (s[j] == '\\') {
            j += 2;
        } else if (s[j] == '`') {
            r = run(s, len, j);
            k = code_close(s, len, j + r, r);
            j = k == NONE ? j + r : k + r;
        } else if (md_footnote(s, len, j)) {
            return j;
        } else {
            j++;
        }
    }
    return NONE;
}

/* Adreça automàtica <http://...>: retorna la posició de '>' o NONE. */
static size_t autolink_end(const char *s, size_t b, size_t i)
{
    static const char *schemes[] = { "http://", "https://", "mailto:" };
    size_t j;

    for (size_t k = 0; k < sizeof schemes / sizeof *schemes; k++) {
        size_t n = strlen(schemes[k]);

        if (i + 1 + n > b || memcmp(s + i + 1, schemes[k], n))
            continue;
        for (j = i + 1 + n; j < b && s[j] != '>' && !is_blank(s[j]); j++)
            ;
        return j < b && s[j] == '>' ? j : NONE;
    }
    return NONE;
}

/* Estils de línia: codi, èmfasi, enllaços i escapaments a [a, b). */
static void inline_style(const char *s, size_t a, size_t b, unsigned char st,
                         unsigned char *out)
{
    size_t i = a, n, k, te, ue;

    while (i < b) {
        char c = s[i];

        if (c == '\\' && i + 1 < b && is_punct((unsigned char)s[i + 1])) {
            out[i] = st | MS_MARK;
            out[i + 1] = st;
            i += 2;
        } else if (c == '`') {
            n = run(s, b, i);
            k = code_close(s, b, i + n, n);
            if (k == NONE) {
                fill(out, i, i + n, st);
            } else {
                fill(out, i, i + n, st | MS_MARK);
                fill(out, i + n, k, st | MS_CODE);
                fill(out, k, k + n, st | MS_MARK);
                n = k + n - i;
            }
            i += n;
        } else if (c == '*' || c == '_') {
            n = run(s, b, i);
            k = NONE;
            if (n <= 3 && i + n < b && !is_blank(s[i + n]) &&
                (c == '*' || i == 0 || !is_word((unsigned char)s[i - 1])))
                k = emph_close(s, b, i + n, c, n);
            if (k == NONE) {
                fill(out, i, i + n, st);
            } else {
                unsigned char add = (n != 2 ? MS_ITALIC : 0) | (n >= 2 ? MS_BOLD : 0);

                fill(out, i, i + n, st | MS_MARK);
                inline_style(s, i + n, k, st | add, out);
                fill(out, k, k + n, st | MS_MARK);
                n = k + n - i;
            }
            i += n;
        } else if (c == '[' && (k = md_footnote(s, b, i))) {
            /* Crida de nota [^1]: l'etiqueta com un enllaç */
            fill(out, i, i + 2, st | MS_MARK);
            fill(out, i + 2, k - 1, st | MS_LINK);
            out[k - 1] = st | MS_MARK;
            i = k;
        } else if ((c == '[' || (c == '!' && i + 1 < b && s[i + 1] == '[')) &&
                   link_end(s, b, i + (c == '!') + 1, &te, &ue)) {
            fill(out, i, i + (c == '!') + 1, st | MS_MARK);
            inline_style(s, i + (c == '!') + 1, te, st | MS_LINK, out);
            fill(out, te, ue + 1, st | MS_MARK);
            i = ue + 1;
        } else if (c == '<' && (k = autolink_end(s, b, i)) != NONE) {
            out[i] = out[k] = st | MS_MARK;
            fill(out, i + 1, k, st | MS_LINK);
            i = k + 1;
        } else {
            out[i++] = st;
        }
    }
}

void md_style(const char *s, size_t len, int kind, int setext, unsigned char *out)
{
    size_t i, p, e;
    unsigned char st = 0;

    memset(out, 0, len);
    switch (MD_BASE(kind)) {
    case MD_EMPTY:
        return;
    case MD_CODE:
        fill(out, 0, len, MS_CODE);
        return;
    case MD_FENCE_OPEN:
    case MD_FENCE_CLOSE:
    case MD_META:
    case MD_HTML:
    case MD_UNDERLINE:
        fill(out, 0, len, MS_MARK);
        return;
    case MD_HEADING:
        i = skip_blanks(s, len, 0);
        p = i + run(s, len, i);
        st = MS_H(p - i > 3 ? 3 : p - i);
        p = skip_blanks(s, len, p);
        fill(out, 0, p, st | MS_MARK);
        /* Tanca opcional: "## Títol ##" */
        for (e = len; e > p && is_blank(s[e - 1]); e--)
            ;
        i = e;
        while (i > p && s[i - 1] == '#')
            i--;
        if (i < e && (i == p || is_blank(s[i - 1]))) {
            fill(out, i, len, st | MS_MARK);
            len = i;
        }
        inline_style(s, p, len, st, out);
        return;
    }

    p = md_prefix(s, len);
    for (i = 0; i < p; i++)
        out[i] = is_blank(s[i]) ? 0 : MS_MARK;
    if (MD_BASE(kind) == MD_QUOTE)
        st = MS_QUOTE;
    else if (setext)
        st = MS_H(setext);
    if ((kind & MD_HARDBREAK) && s[len - 1] == '\\')
        out[--len] = st | MS_MARK;
    inline_style(s, p, len, st, out);
    if (MD_BASE(kind) == MD_TABLE)
        for (i = p; i < len; i++)
            if (s[i] == '|' && !(out[i] & (MS_CODE | MS_MARK)) && (i == 0 || s[i - 1] != '\\'))
                out[i] |= MS_MARK;
}
