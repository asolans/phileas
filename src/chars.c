#include "chars.h"
#include "utf8.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static const struct { const char *name; const wchar_t ch; } names[] = {
    { "guio-llarg", L'—' },   { "guio-mitja", L'–' },    { "punt-volat", L'·' },
    { "punts", L'…' },        { "obre", L'«' },          { "tanca", L'»' },
    { "cometa-obre", L'“' },  { "cometa-tanca", L'”' },  { "apostrof", L'’' },
    { "c-majuscula", L'Ç' },  { "c-minuscula", L'ç' },   { "ela-geminada", L'ŀ' },
    { "grau", L'°' },         { "euro", L'€' },          { "lliura", L'£' },
    { "copyright", L'©' },    { "seccio", L'§' },        { "paragraf", L'¶' },
    { "fletxa", L'→' },       { "inversa", L'¿' },       { "exclamacio", L'¡' },
    { "ordinal-a", L'ª' },    { "ordinal-o", L'º' },     { "multiplica", L'×' },
};

size_t char_lookup(const char *arg, char *out, size_t size)
{
    size_t len, n;
    wchar_t wc;

    while (*arg == ' ')
        arg++;
    len = strlen(arg);
    while (len && arg[len - 1] == ' ')
        len--;
    if (!len || size < 5)
        return 0;
    for (size_t i = 0; i < sizeof names / sizeof *names; i++)
        if (strlen(names[i].name) == len && !strncmp(arg, names[i].name, len)) {
            n = utf8_encode(names[i].ch, out);
            out[n] = '\0';
            return n;
        }
    /* Codi hexadecimal, amb U+ o 0x opcionals */
    {
        char buf[16], *end;
        const char *p = arg;
        unsigned long v;

        if (len < sizeof buf) {
            memcpy(buf, arg, len);
            buf[len] = '\0';
            p = buf;
            if (!strncasecmp(p, "u+", 2))
                p += 2;
            else if (!strncasecmp(p, "0x", 2))
                p += 2;
            if (*p && strspn(p, "0123456789abcdefABCDEF") == strlen(p) && strlen(p) <= 6) {
                v = strtoul(p, &end, 16);
                if (*end == '\0' && v >= 0x20 && v <= 0x10FFFF && !(v >= 0xD800 && v <= 0xDFFF) &&
                    !(v >= 0x7F && v < 0xA0)) {
                    n = utf8_encode((wchar_t)v, out);
                    out[n] = '\0';
                    return n;
                }
                /* «1234» que no és un codi vàlid: res. «a», «e»… van al cas següent */
                if (strlen(p) > 1)
                    return 0;
            }
        }
    }
    /* Un sol caràcter */
    n = utf8_decode(arg, len, 0, &wc);
    if (wc != UTF8_INVALID && n == len) {
        memcpy(out, arg, n);
        out[n] = '\0';
        return n;
    }
    return 0;
}

const char *char_names(void)
{
    static char list[512];

    if (!list[0])
        for (size_t i = 0; i < sizeof names / sizeof *names; i++) {
            if (i)
                strcat(list, " ");
            strcat(list, names[i].name);
        }
    return list;
}
