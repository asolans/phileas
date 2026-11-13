#include "rc.h"

#include "theme.h"
#include "util.h"

#include <stdio.h>
#include <string.h>

static void read_file(const char *path, cm_rc_fn fn, void *ud)
{
    FILE *f = fopen(path, "r");
    char line[1024];

    if (!f)
        return;
    while (fgets(line, sizeof line, f)) {
        char *k = line, *eq = strchr(line, '='), *v;
        size_t l;

        while (*k == ' ' || *k == '\t')
            k++;
        if (*k == '#' || !eq)
            continue;
        *eq = '\0';
        v = eq + 1;
        for (l = strlen(k); l && (k[l - 1] == ' ' || k[l - 1] == '\t'); l--)
            k[l - 1] = '\0';
        while (*v == ' ' || *v == '\t')
            v++;
        for (l = strlen(v); l && (v[l - 1] == '\n' || v[l - 1] == '\r' || v[l - 1] == ' ' || v[l - 1] == '\t'); l--)
            v[l - 1] = '\0';
        if (*k)
            fn(k, v, ud);
    }
    fclose(f);
}

void cm_rc_read(cm_rc_fn fn, void *ud)
{
    char dir[512], path[640];

    read_file("/etc/phileas/phileasrc", fn, ud);
    config_dir(dir, sizeof dir);
    snprintf(path, sizeof path, "%s/phileasrc", dir);
    read_file(path, fn, ud);
}

static const struct {
    const char *key;
    int pal;
} color_keys[] = {
    { "color-capcalera-1", PAL_H1 }, { "color-capcalera-2", PAL_H2 },
    { "color-capcalera-3", PAL_H3 }, { "color-codi", PAL_CODE },
    { "color-cita", PAL_QUOTE },     { "color-error", PAL_ERROR },
    { "color-marcadors", PAL_MARK },
    { "color-text", PAL_TEXT },      { "color-fons", PAL_BG },
    { "color-barra", PAL_BAR },      { "color-barra-fons", PAL_BAR_BG },
    { "color-seleccio", PAL_SEL },   { "color-seleccio-fons", PAL_SEL_BG },
    { "color-menu", PAL_MENU },      { "color-menu-fons", PAL_MENU_BG },
};

int cm_rc_color(const char *key, const char *value, short over[PAL_N])
{
    for (size_t i = 0; i < sizeof color_keys / sizeof *color_keys; i++)
        if (!strcmp(key, color_keys[i].key)) {
            short c = theme_color_parse(value);

            if (c != -2)
                over[color_keys[i].pal] = c;
            return 1;
        }
    return 0;
}

void cm_rc_color_reset(short over[PAL_N])
{
    for (int i = 0; i < PAL_N; i++)
        over[i] = CM_NO_COLOR;
}

void cm_rc_apply(short pal[PAL_N], const short over[PAL_N])
{
    for (int i = 0; i < PAL_N; i++)
        if (over[i] != CM_NO_COLOR)
            pal[i] = over[i];
}
