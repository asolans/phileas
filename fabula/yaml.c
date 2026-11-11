#include "yaml.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int is_fence(const char *s, size_t n)
{
    while (n && (s[n - 1] == '\r' || s[n - 1] == ' ' || s[n - 1] == '\t'))
        n--;
    return n == 3 && (!memcmp(s, "---", 3) || !memcmp(s, "...", 3));
}

/* Capçalera: [*a, *z) són les línies entre les tanques; *after és el byte
 * després de la tanca de tancament. */
static int fm_find(const char *s, size_t n, size_t *a, size_t *z, size_t *after)
{
    size_t i = 0;

    while (i < n && s[i] != '\n')
        i++;
    if (i >= n || !is_fence(s, i))
        return 0;
    if (n >= 3 && memcmp(s, "---", 3))
        return 0;
    *a = ++i;
    while (i < n) {
        size_t e = i;

        while (e < n && s[e] != '\n')
            e++;
        if (is_fence(s + i, e - i)) {
            *z = i;
            *after = e < n ? e + 1 : e;
            return 1;
        }
        i = e < n ? e + 1 : e;
    }
    return 0;
}

/* Clau d'una línia plana, o NULL si no ho és. */
static int line_key(const char *l, size_t n, const char *key, size_t *vstart, size_t *vend)
{
    size_t kl = strlen(key), c;

    if (n == 0 || l[0] == '#' || l[0] == ' ' || l[0] == '\t' || l[0] == '-')
        return 0;
    for (c = 0; c < n && l[c] != ':'; c++)
        ;
    if (c >= n)
        return 0;
    if (c != kl || memcmp(l, key, kl))
        return 0;
    if (c + 1 < n && l[c + 1] != ' ' && l[c + 1] != '\t' && l[c + 1] != '\r')
        return 0;
    *vstart = c + 1;
    *vend = n;
    while (*vend > *vstart && (l[*vend - 1] == '\r' || l[*vend - 1] == ' ' || l[*vend - 1] == '\t'))
        (*vend)--;
    while (*vstart < *vend && (l[*vstart] == ' ' || l[*vstart] == '\t'))
        (*vstart)++;
    return 1;
}

/* Treu les cometes d'un valor: "a \" b", 'a '' b' */
static size_t unquote(const char *v, size_t n, char *out)
{
    size_t o = 0, i;

    if (n >= 2 && v[0] == '"' && v[n - 1] == '"') {
        for (i = 1; i + 1 < n; i++) {
            if (v[i] == '\\' && i + 2 < n)
                i++;
            out[o++] = v[i];
        }
    } else if (n >= 2 && v[0] == '\'' && v[n - 1] == '\'') {
        for (i = 1; i + 1 < n; i++) {
            if (v[i] == '\'' && v[i + 1] == '\'')
                i++;
            out[o++] = v[i];
        }
    } else {
        memcpy(out, v, n);
        o = n;
    }
    out[o] = '\0';
    return o;
}

int yaml_get(const char *text, size_t len, const char *key, char *out, size_t outsz)
{
    size_t a, z, after, i;

    if (outsz)
        out[0] = '\0';
    if (!fm_find(text, len, &a, &z, &after))
        return 0;
    for (i = a; i < z;) {
        size_t e = i, vs, ve;

        while (e < z && text[e] != '\n')
            e++;
        if (line_key(text + i, e - i, key, &vs, &ve)) {
            char *tmp = xmalloc(ve - vs + 1);
            size_t n = unquote(text + i + vs, ve - vs, tmp);

            if (outsz) {
                if (n >= outsz)
                    n = outsz - 1;
                memcpy(out, tmp, n);
                out[n] = '\0';
            }
            free(tmp);
            return 1;
        }
        i = e + 1;
    }
    return 0;
}

size_t yaml_list(const char *value, char ***items)
{
    size_t n = strlen(value), cnt = 0, cap = 4, i;
    char **v;

    *items = NULL;
    while (n && (value[n - 1] == ' ' || value[n - 1] == '\t'))
        n--;
    if (n < 2 || value[0] != '[' || value[n - 1] != ']')
        return 0;
    v = xmalloc(cap * sizeof *v);
    for (i = 1; i < n - 1;) {
        size_t s = i, e;
        char q = 0;

        while (s < n - 1 && (value[s] == ' ' || value[s] == '\t'))
            s++;
        e = s;
        while (e < n - 1 && (q || value[e] != ',')) {
            if (q && value[e] == '\\' && q == '"')
                e++;
            else if (value[e] == q)
                q = 0;
            else if (!q && (value[e] == '"' || value[e] == '\''))
                q = value[e];
            e++;
        }
        if (e > n - 1)
            e = n - 1;
        {
            size_t t = e;
            char *it;

            while (t > s && (value[t - 1] == ' ' || value[t - 1] == '\t'))
                t--;
            if (t > s) {
                it = xmalloc(t - s + 1);
                unquote(value + s, t - s, it);
                if (cnt == cap)
                    v = xrealloc(v, (cap *= 2) * sizeof *v);
                v[cnt++] = it;
            }
        }
        i = e + 1;
    }
    if (!cnt) {
        free(v);
        v = NULL;
    }
    *items = v;
    return cnt;
}

void yaml_list_free(char **items, size_t n)
{
    for (size_t i = 0; i < n; i++)
        free(items[i]);
    free(items);
}

/* Cometes quan el valor no es pot escriure tal qual */
static char *format_value(const char *v, int raw)
{
    size_t n = strlen(v), o = 0;
    char *out = xmalloc(2 * n + 3);
    int quote = 0;

    if (raw) {
        memcpy(out, v, n + 1);
        return out;
    }
    if (n == 0 || v[0] == ' ' || v[n - 1] == ' ' || strchr("[]{}#&*!|>'\"%@`-?:,", v[0]) ||
        strstr(v, ": ") || strstr(v, " #") || strchr(v, '\n') || strchr(v, '\r') ||
        v[n - 1] == ':')
        quote = 1;
    if (!quote) {
        memcpy(out, v, n + 1);
        return out;
    }
    out[o++] = '"';
    for (size_t i = 0; i < n; i++) {
        if (v[i] == '"' || v[i] == '\\')
            out[o++] = '\\';
        out[o++] = (v[i] == '\n' || v[i] == '\r') ? ' ' : v[i];
    }
    out[o++] = '"';
    out[o] = '\0';
    return out;
}

char *yaml_set(const char *text, size_t len, const char *key, const char *value,
               int raw, size_t *outlen)
{
    char *val = format_value(value, raw), *res;
    size_t a, z, after, kl = strlen(key), vl = strlen(val), i;
    int crlf = len && memchr(text, '\r', len < 200 ? len : 200) != NULL;
    const char *nl = crlf ? "\r\n" : "\n";

    if (!fm_find(text, len, &a, &z, &after)) {
        size_t rl = 3 + (size_t)strlen(nl) * 3 + kl + 2 + vl + 3 + len + 2;

        res = xmalloc(rl + 1);
        *outlen = (size_t)sprintf(res, "---%s%s: %s%s---%s", nl, key, val, nl, nl);
        if (len) {
            *outlen += (size_t)sprintf(res + *outlen, "%s", nl);
            memcpy(res + *outlen, text, len);
            *outlen += len;
        }
        res[*outlen] = '\0';
        free(val);
        return res;
    }
    res = xmalloc(len + kl + vl + 8);
    for (i = a; i < z;) {
        size_t e = i, vs, ve;

        while (e < z && text[e] != '\n')
            e++;
        if (line_key(text + i, e - i, key, &vs, &ve)) {
            int cr = e > i && text[e - 1] == '\r';
            size_t o = i;

            memcpy(res, text, o);
            memcpy(res + o, key, kl), o += kl;
            res[o++] = ':', res[o++] = ' ';
            memcpy(res + o, val, vl), o += vl;
            if (cr)
                res[o++] = '\r';
            memcpy(res + o, text + e, len - e);
            *outlen = o + (len - e);
            res[*outlen] = '\0';
            free(val);
            return res;
        }
        i = e + 1;
    }
    /* no hi és: abans de la tanca de tancament */
    {
        size_t o = z;

        memcpy(res, text, o);
        memcpy(res + o, key, kl), o += kl;
        res[o++] = ':', res[o++] = ' ';
        memcpy(res + o, val, vl), o += vl;
        memcpy(res + o, nl, strlen(nl)), o += strlen(nl);
        memcpy(res + o, text + z, len - z);
        *outlen = o + (len - z);
        res[*outlen] = '\0';
    }
    free(val);
    return res;
}
