#include "front.h"

#include <string.h>

static void trim(const char **s, size_t *n)
{
    while (*n && (**s == ' ' || **s == '\t')) {
        (*s)++;
        (*n)--;
    }
    while (*n && ((*s)[*n - 1] == ' ' || (*s)[*n - 1] == '\t' || (*s)[*n - 1] == '\r'))
        (*n)--;
}

static void unquote(const char **s, size_t *n)
{
    /* 'valor' # comentari: es talla a la cometa de tancament */
    if (*n >= 2 && ((*s)[0] == '"' || (*s)[0] == '\'')) {
        const char *z = memchr(*s + 1, (*s)[0], *n - 1);

        if (z) {
            (*s)++;
            *n = z - *s;
            return;
        }
    }
    if (*n >= 2 && (((*s)[0] == '"' && (*s)[*n - 1] == '"') || ((*s)[0] == '\'' && (*s)[*n - 1] == '\''))) {
        (*s)++;
        *n -= 2;
    }
}

/* Comentari al final d'un valor sense cometes: « #…» */
static void uncomment(const char *s, size_t *n)
{
    if (*n && (s[0] == '"' || s[0] == '\'' || s[0] == '['))
        return;
    for (size_t i = 1; i < *n; i++)
        if (s[i] == '#' && (s[i - 1] == ' ' || s[i - 1] == '\t')) {
            *n = i;
            return;
        }
}

static void inline_list(const char *k, size_t kl, const char *s, size_t n, cm_prop_fn fn, void *ud)
{
    size_t i = 1, idx = 0;          /* s[0] = '[' */

    if (n && s[n - 1] == ']')
        n--;
    while (i < n) {
        size_t a, b;
        const char *v;
        size_t vl;

        while (i < n && (s[i] == ' ' || s[i] == '\t'))
            i++;
        if (i >= n)
            break;
        a = i;
        if (s[i] == '"' || s[i] == '\'') {
            char q = s[i++];

            while (i < n && s[i] != q)
                i++;
            if (i < n)
                i++;
        } else {
            int depth = 0;          /* [[Nota]] dins de la llista sense cometes */

            while (i < n && (s[i] != ',' || depth)) {
                if (s[i] == '[')
                    depth++;
                else if (s[i] == ']' && depth)
                    depth--;
                i++;
            }
        }
        b = i;
        while (i < n && s[i] != ',')
            i++;
        i++;
        v = s + a;
        vl = b - a;
        trim(&v, &vl);
        unquote(&v, &vl);
        if (vl && fn)
            fn(k, kl, v, vl, (int)idx++, ud);
    }
}

size_t cm_front(const char *text, size_t len, size_t *body_line, cm_prop_fn fn, void *ud)
{
    const char *p = text, *end = text + len;
    const char *key = NULL;
    size_t kl = 0, line = 1;
    int idx = 0;

    if (body_line)
        *body_line = 1;
    if (len >= 3 && !memcmp(p, "\xEF\xBB\xBF", 3))
        p += 3;
    if (end - p < 4 || memcmp(p, "---", 3) || (p[3] != '\n' && p[3] != '\r'))
        return 0;
    p = (const char *)memchr(p, '\n', end - p) + 1;
    line++;
    while (p < end) {
        const char *e = memchr(p, '\n', end - p);
        const char *l = p;
        size_t n = (e ? e : end) - p;

        if (n && l[n - 1] == '\r')
            n--;
        if ((n == 3 && (!memcmp(l, "---", 3) || !memcmp(l, "...", 3)))) {
            if (body_line)
                *body_line = line + 1;
            return e ? (size_t)(e + 1 - text) : len;
        }
        if (n && l[0] != '#') {
            const char *s = l;
            size_t sn = n;

            trim(&s, &sn);
            if (sn >= 1 && s[0] == '-' && (sn == 1 || s[1] == ' ') && key && l[0] != '\0') {
                const char *v = s + 1;          /* element d'una llista en bloc */
                size_t vl = sn - 1;

                trim(&v, &vl);
                uncomment(v, &vl);
                trim(&v, &vl);
                unquote(&v, &vl);
                if (vl && fn)
                    fn(key, kl, v, vl, idx++, ud);
            } else if (l[0] != ' ' && l[0] != '\t') {
                const char *c = memchr(l, ':', n);

                key = NULL;
                if (c && (c + 1 == l + n || c[1] == ' ' || c[1] == '\t')) {
                    const char *v = c + 1;
                    size_t vl = l + n - v;
                    const char *k = l;
                    size_t kln = c - l;

                    trim(&k, &kln);
                    unquote(&k, &kln);
                    trim(&v, &vl);
                    if (!vl) {
                        key = k;            /* pot venir una llista en bloc */
                        kl = kln;
                        idx = 0;
                    } else if (v[0] == '[' && !(vl >= 2 && v[1] == '[')) {
                        inline_list(k, kln, v, vl, fn, ud);
                    } else {
                        uncomment(v, &vl);
                        trim(&v, &vl);
                        unquote(&v, &vl);
                        if (fn)
                            fn(k, kln, v, vl, -1, ud);
                    }
                }
            }
        }
        if (!e)
            break;
        p = e + 1;
        line++;
    }
    return 0;                       /* capçalera sense tancar: no n'hi ha */
}
