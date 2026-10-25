#include "util.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void (*die_hook)(void);

void die(const char *fmt, ...)
{
    va_list ap;

    if (die_hook)
        die_hook();
    fputs("phileas: ", stderr);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(1);
}

void *xmalloc(size_t n)
{
    void *p = malloc(n ? n : 1);

    if (!p)
        die("memòria esgotada");
    return p;
}

void *xrealloc(void *p, size_t n)
{
    p = realloc(p, n ? n : 1);
    if (!p)
        die("memòria esgotada");
    return p;
}

char *xstrdup(const char *s)
{
    size_t n = strlen(s) + 1;

    return memcpy(xmalloc(n), s, n);
}

void config_dir(char *out, size_t size)
{
    const char *dir = getenv("PHILEAS_CONFIG"), *xdg = getenv("XDG_CONFIG_HOME");
    const char *home = getenv("HOME");

    if (dir && *dir)
        snprintf(out, size, "%s", dir);
    else if (xdg && *xdg)
        snprintf(out, size, "%s/phileas", xdg);
    else
        snprintf(out, size, "%s/.config/phileas", home && *home ? home : ".");
}
