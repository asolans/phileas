#include "fold.h"

#include "utf8.h"
#include "util.h"

#include <string.h>
#include <wctype.h>

/* Minúscula d'un caràcter llatí, sense locale */
static wchar_t lower(wchar_t c)
{
    if (c < 0x80)
        return (c >= 'A' && c <= 'Z') ? c + 32 : c;
    if (c >= 0xC0 && c <= 0xDE && c != 0xD7)       /* À…Þ (menys ×) */
        return c + 32;
    if (c >= 0x100 && c <= 0x137 && !(c & 1))      /* Ā…ķ: parelles */
        return c + 1;
    if (c >= 0x139 && c <= 0x148 && (c & 1))       /* Ĺ…ň */
        return c + 1;
    if (c >= 0x14A && c <= 0x177 && !(c & 1))      /* Ŋ…ŷ */
        return c + 1;
    if (c == 0x178)                                /* Ÿ */
        return 0xFF;
    if (c == 0x179 || c == 0x17B || c == 0x17D)    /* Ź Ż Ž */
        return c + 1;
    return (wchar_t)towlower((wint_t)c);
}

/* Equivalent sense accents d'una minúscula, o NULL si es queda igual */
static const char *strip(wchar_t c)
{
    switch (c) {
    case 0xE0: case 0xE1: case 0xE2: case 0xE3: case 0xE4: case 0xE5: return "a";
    case 0xE7: return "c";
    case 0xE8: case 0xE9: case 0xEA: case 0xEB: return "e";
    case 0xEC: case 0xED: case 0xEE: case 0xEF: return "i";
    case 0xF1: return "n";
    case 0xF2: case 0xF3: case 0xF4: case 0xF5: case 0xF6: case 0xF8: return "o";
    case 0xF9: case 0xFA: case 0xFB: case 0xFC: return "u";
    case 0xFD: case 0xFF: return "y";
    case 0xE6: return "ae";
    case 0xDF: return "ss";
    case 0x153: return "oe";
    case 0x140: return "l";      /* ŀ */
    case 0xB7: return "";        /* punt volat de la l·l */
    case 0x2027: return "";      /* ‧ (punt volat alternatiu) */
    default: return NULL;
    }
}

static char *convert(const char *s, size_t len, int fold)
{
    char *out = xmalloc(4 * len + 1);
    size_t i = 0, o = 0;

    while (i < len) {
        unsigned char b = (unsigned char)s[i];
        wchar_t c;
        size_t k;

        if (b < 0x80) {
            out[o++] = (b >= 'A' && b <= 'Z') ? (char)(b + 32) : (char)b;
            i++;
            continue;
        }
        k = utf8_decode(s, len, i, &c);
        if (!k)
            k = 1;
        if (c == UTF8_INVALID) {
            memcpy(out + o, s + i, k);
            o += k;
            i += k;
            continue;
        }
        i += k;
        c = lower(c);
        if (fold) {
            const char *r = strip(c);

            if (r) {
                size_t n = strlen(r);

                memcpy(out + o, r, n);
                o += n;
                continue;
            }
        }
        o += utf8_encode(c, out + o);
    }
    out[o] = '\0';
    return out;
}

char *cm_lower(const char *s, size_t len) { return convert(s, len, 0); }
char *cm_fold(const char *s, size_t len) { return convert(s, len, 1); }
