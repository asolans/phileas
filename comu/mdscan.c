#include "mdscan.h"

#include "utf8.h"

#include <string.h>

static const char *find2(const char *s, size_t n, char a, char b)
{
    for (size_t i = 0; i + 1 < n; i++)
        if (s[i] == a && s[i + 1] == b)
            return s + i;
    return NULL;
}

static const char *findstr(const char *s, size_t n, const char *needle)
{
    size_t m = strlen(needle);

    for (size_t i = 0; i + m <= n; i++)
        if (!memcmp(s + i, needle, m))
            return s + i;
    return NULL;
}

/* Caràcter d'etiqueta que comença a s[i]; retorna la seva longitud o 0 */
static size_t tag_char(const char *s, size_t n, size_t i)
{
    unsigned char b = (unsigned char)s[i];
    wchar_t c;
    size_t k;

    if (b < 0x80) {
        if ((b >= 'a' && b <= 'z') || (b >= 'A' && b <= 'Z') || (b >= '0' && b <= '9') ||
            b == '_' || b == '-' || b == '/')
            return 1;
        return 0;
    }
    k = utf8_decode(s, n, i, &c);
    if (!k || c == UTF8_INVALID)
        return 0;
    switch (c) {
    case 0xA0: case 0xAB: case 0xBB: case 0xBF: case 0xA1:           /* nbsp « » ¿ ¡ */
    case 0x2013: case 0x2014: case 0x2026: case 0x2018: case 0x2019: /* – — … ‘ ’ */
    case 0x201C: case 0x201D: case 0x2022: case 0x3000:              /* “ ” • */
        return 0;
    default:
        return k;
    }
}

int cm_tag_valid(const char *s, size_t len)
{
    int letter = 0;

    if (!len)
        return 0;
    for (size_t i = 0; i < len;) {
        size_t k = tag_char(s, len, i);

        if (!k)
            return 0;
        if (!(k == 1 && ((s[i] >= '0' && s[i] <= '9'))))
            letter = 1;
        i += k;
    }
    return letter;
}

static int is_id(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-';
}

/* [[ … ]] o ![[ … ]] a partir de p (que apunta a «[[»); retorna on acaba o NULL */
static const char *wikilink(const char *p, const char *end, int embed, CmRef *r)
{
    const char *a = p + 2, *z = find2(a, end - a, ']', ']');
    const char *t, *te, *bar = NULL, *hash = NULL;

    if (!z || find2(a, z - a, '[', '['))
        return NULL;
    for (t = a; t < z; t++) {
        if (*t == '|' && !bar)
            bar = t;
        else if (*t == '#' && !hash && !bar)
            hash = t;
    }
    te = hash ? hash : bar ? bar : z;
    if (te > a && te[-1] == '\\')           /* [[A\|b]] dins de les taules */
        te--;
    r->embed = embed;
    r->target = a;
    r->target_len = te - a;
    if (r->target_len >= 3 && !memcmp(te - 3, ".md", 3))
        r->target_len -= 3;
    while (r->target_len && r->target[r->target_len - 1] == ' ')
        r->target_len--;
    r->section = NULL;
    r->section_len = 0;
    r->block = 0;
    if (hash) {
        const char *se = bar ? bar : z;

        if (se > hash + 1 && se[-1] == '\\')
            se--;
        r->section = hash + 1;
        if (r->section < se && *r->section == '^') {
            r->block = 1;
            r->section++;
        }
        r->section_len = se - r->section;
    }
    r->alias = bar ? bar + 1 : NULL;
    r->alias_len = bar ? (size_t)(z - bar - 1) : 0;
    return z + 2;
}

/* [text](destí.md) intern; retorna on acaba o NULL si no ho és */
static const char *mdlink(const char *p, const char *end, CmRef *r, char *buf, size_t bufsz)
{
    const char *cb = memchr(p, ']', end - p), *ue, *u, *hash;
    size_t o = 0;

    if (!cb || cb + 1 >= end || cb[1] != '(')
        return NULL;
    u = cb + 2;
    ue = memchr(u, ')', end - u);
    if (!ue || findstr(u, ue - u, "://") || findstr(u, ue - u, "mailto:"))
        return NULL;
    hash = memchr(u, '#', ue - u);
    if (!hash)
        hash = ue;
    if (hash - u < 4 || memcmp(hash - 3, ".md", 3))
        return NULL;
    for (const char *s = u; s < hash - 3 && o + 1 < bufsz; s++) {   /* %20 → espai */
        if (*s == '%' && s + 2 < hash && s[1] == '2' && s[2] == '0') {
            buf[o++] = ' ';
            s += 2;
        } else
            buf[o++] = *s;
    }
    buf[o] = '\0';
    r->embed = 0;
    r->target = buf;
    r->target_len = o;
    r->section = hash < ue ? hash + 1 : NULL;
    r->section_len = hash < ue ? (size_t)(ue - hash - 1) : 0;
    r->block = r->section && r->section_len && *r->section == '^';
    if (r->block) {
        r->section++;
        r->section_len--;
    }
    r->alias = p + 1;
    r->alias_len = cb - p - 1;
    return ue + 1;
}

typedef struct {
    int fence;          /* dins d'un bloc de codi: el caràcter (` o ~) */
    size_t fence_n;
    int pct;            /* dins de %% … %% */
    int html;           /* dins de <!-- … --> */
} State;

static void scan_line(const char *l, size_t n, size_t line, State *st, cm_ref_fn fn, void *ud)
{
    const char *p = l, *end = l + n;
    CmRef r;
    char buf[1024];

    memset(&r, 0, sizeof r);
    r.line = line;
    r.ctx = l;
    r.ctx_len = n;

    /* bloc de codi */
    {
        const char *s = l;
        size_t sp = 0;

        while (s < end && *s == ' ' && sp < 4) {
            s++;
            sp++;
        }
        if (sp < 4 && s < end && (*s == '`' || *s == '~') && !st->pct && !st->html) {
            char c = *s;
            size_t k = 0;

            while (s + k < end && s[k] == c)
                k++;
            if (k >= 3) {
                if (!st->fence) {
                    st->fence = c;
                    st->fence_n = k;
                    return;
                }
                if (st->fence == c && k >= st->fence_n) {
                    st->fence = 0;
                    return;
                }
            }
        }
        if (st->fence)
            return;
    }

    while (p < end) {
        if (st->pct) {
            const char *z = find2(p, end - p, '%', '%');

            if (!z)
                return;
            st->pct = 0;
            p = z + 2;
            continue;
        }
        if (st->html) {
            const char *z = findstr(p, end - p, "-->");

            if (!z)
                return;
            st->html = 0;
            p = z + 3;
            continue;
        }
        switch (*p) {
        case '`': {
            size_t k = 0;
            const char *q;

            while (p + k < end && p[k] == '`')
                k++;
            for (q = p + k; q < end; q++)
                if (*q == '`') {
                    size_t m = 0;

                    while (q + m < end && q[m] == '`')
                        m++;
                    if (m == k)
                        break;
                    q += m - 1;
                }
            p = q < end ? q + k : p + k;
            continue;
        }
        case '%':
            if (p + 1 < end && p[1] == '%') {
                st->pct = 1;
                p += 2;
                continue;
            }
            break;
        case '<':
            if (end - p >= 4 && !memcmp(p, "<!--", 4)) {
                st->html = 1;
                p += 4;
                continue;
            }
            break;
        case '!':
            if (end - p >= 3 && p[1] == '[' && p[2] == '[') {
                const char *z = wikilink(p + 1, end, 1, &r);

                if (z) {
                    r.kind = CM_LINK;
                    fn(&r, ud);
                    p = z;
                    continue;
                }
            }
            break;
        case '[':
            if (p + 1 < end && p[1] == '[') {
                const char *z = wikilink(p, end, 0, &r);

                if (z) {
                    r.kind = CM_LINK;
                    fn(&r, ud);
                    p = z;
                    continue;
                }
            } else {
                const char *z = mdlink(p, end, &r, buf, sizeof buf);

                if (z) {
                    r.kind = CM_LINK;
                    fn(&r, ud);
                    p = z;
                    continue;
                }
            }
            break;
        case '#':
            if (p == l || p[-1] == ' ' || p[-1] == '\t') {
                const char *q = p + 1;

                while (q < end) {
                    size_t k = tag_char(q, end - q, 0);

                    if (!k)
                        break;
                    q += k;
                }
                if (q > p + 1 && cm_tag_valid(p + 1, q - p - 1)) {
                    r.kind = CM_TAG;
                    r.target = p + 1;
                    r.target_len = q - p - 1;
                    r.section = r.alias = NULL;
                    r.section_len = r.alias_len = 0;
                    r.embed = r.block = 0;
                    fn(&r, ud);
                    p = q;
                    continue;
                }
                while (p < end && *p == '#')    /* ##, capçaleres */
                    p++;
                continue;
            }
            break;
        case '^':
            if (p == l || p[-1] == ' ') {
                const char *q = p + 1, *t;

                while (q < end && is_id(*q))
                    q++;
                for (t = q; t < end && (*t == ' ' || *t == '\t' || *t == '\r'); t++)
                    ;
                if (q > p + 1 && t == end) {
                    r.kind = CM_BLOCK;
                    r.target = p + 1;
                    r.target_len = q - p - 1;
                    r.section = r.alias = NULL;
                    r.section_len = r.alias_len = 0;
                    r.embed = r.block = 0;
                    fn(&r, ud);
                    return;
                }
            }
            break;
        default:
            break;
        }
        p++;
    }
}

void cm_scan(const char *text, size_t len, size_t first_line, cm_ref_fn fn, void *ud)
{
    State st = { 0, 0, 0, 0 };
    size_t line = first_line;
    const char *p = text, *end = text + len;

    while (p < end) {
        const char *e = memchr(p, '\n', end - p);
        size_t n = (e ? e : end) - p;

        if (n && p[n - 1] == '\r')
            n--;
        scan_line(p, n, line, &st, fn, ud);
        if (!e)
            break;
        p = e + 1;
        line++;
    }
}

void cm_scan_value(const char *value, size_t len, cm_ref_fn fn, void *ud)
{
    const char *p = value, *end = value + len;
    CmRef r;

    memset(&r, 0, sizeof r);
    r.ctx = value;
    r.ctx_len = len;
    while ((p = find2(p, end - p, '[', '['))) {
        const char *z = wikilink(p, end, 0, &r);

        if (!z) {
            p += 2;
            continue;
        }
        r.kind = CM_LINK;
        fn(&r, ud);
        p = z;
    }
}
