#include "pairs.h"
#include "utf8.h"

#include <ctype.h>
#include <string.h>
#include <wctype.h>

#define MAXDEPTH 64

static int opener(wchar_t c)
{
    return c == L'«' || c == L'(' || c == L'[' || c == L'“';
}

static wchar_t partner(wchar_t c)
{
    switch (c) {
    case L'«': return L'»';
    case L'(': return L')';
    case L'[': return L']';
    case L'“': return L'”';
    }
    return 0;
}

static int closer(wchar_t c)
{
    return c == L'»' || c == L')' || c == L']' || c == L'”';
}

/* Línia que comença amb un numeral de llista: «1)», «a)», «(1)» és diferent */
static int list_marker(const char *s, size_t i)
{
    size_t k = 0;

    while (k < i && (s[k] == ' ' || s[k] == '\t'))
        k++;
    if (k >= i)
        return 0;
    for (size_t j = k; j < i; j++)
        if (!isalnum((unsigned char)s[j]))
            return 0;
    return i - k <= 3;
}

int pairs_check(const char *s, size_t len, size_t *pos, const char **why)
{
    size_t stack[MAXDEPTH], i = 0;
    wchar_t kind[MAXDEPTH];
    int depth = 0, quotes = 0, dashes = 0, dialog = 0;
    size_t qpos = 0, k = 0;

    while (k < len && (s[k] == ' ' || s[k] == '\t'))
        k++;
    if (k < len && !strncmp(s + k, "—", strlen("—")))
        dialog = 1;
    while (i < len) {
        wchar_t c;
        size_t n = utf8_decode(s, len, i, &c);

        if (c == UTF8_INVALID || !n) {
            i++;
            continue;
        }
        if (c == L'`') {                       /* codi en línia: es salta fins al tancament */
            size_t j = i + 1;

            while (j < len && s[j] != '`')
                j++;
            if (j < len) {
                i = j + 1;
                continue;
            }
        } else if (opener(c)) {
            if (depth < MAXDEPTH) {
                stack[depth] = i;
                kind[depth] = c;
            }
            depth++;
        } else if (closer(c)) {
            int emoticon = c == L')' && i > 0 && (s[i - 1] == ':' || s[i - 1] == ';');
            int marker = c == L')' && depth == 0 && list_marker(s, i);

            if (!emoticon && !marker) {
                if (depth == 0) {
                    *pos = i;
                    *why = c == L'»' ? "» sense obrir" : c == L')' ? ") sense obrir"
                         : c == L']' ? "] sense obrir" : "” sense obrir";
                    return 1;
                }
                if (depth <= MAXDEPTH && partner(kind[depth - 1]) != c) {
                    *pos = stack[depth - 1];
                    *why = kind[depth - 1] == L'«' ? "« tancat amb un altre signe"
                         : kind[depth - 1] == L'(' ? "( tancat amb un altre signe"
                         : kind[depth - 1] == L'[' ? "[ tancat amb un altre signe"
                         : "“ tancat amb un altre signe";
                    return 1;
                }
                depth--;
            }
        } else if (c == L'"') {
            if (!(quotes++ & 1))
                qpos = i;
        } else if (c == L'—') {
            /* «——» o «---» no són diàleg */
            dashes++;
        }
        i += n;
    }
    if (depth > 0) {
        *pos = stack[0];
        *why = kind[0] == L'«' ? "« sense tancar" : kind[0] == L'(' ? "( sense tancar"
             : kind[0] == L'[' ? "[ sense tancar" : "“ sense tancar";
        return 1;
    }
    if (quotes & 1) {
        *pos = qpos;
        *why = "\" sense tancar";
        return 1;
    }
    /* Diàleg: un guió obre; cada acotació en gasta dos més (obre i tanca) */
    if (dialog && dashes % 2 == 0) {
        size_t last = 0;

        for (i = 0; i < len;) {
            wchar_t c;
            size_t n = utf8_decode(s, len, i, &c);

            if (c == L'—')
                last = i;
            i += n ? n : 1;
        }
        *pos = last;
        *why = "acotació de diàleg sense tancar (falta un guió)";
        return 1;
    }
    return 0;
}
