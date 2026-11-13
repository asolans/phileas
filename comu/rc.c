#include "rc.h"

#include "theme.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void read_file(const char *path, cm_rc_fn fn, void *ud)
{
    FILE *f = fopen(path, "r");
    char line[1024];

    if (!f)
        return;
    while (fgets(line, sizeof line, f)) {
        char *k = line, *eq, *v, *hash;
        size_t l;

        /* Comentaris: # al principi o després d'un espai (com a Phileas) */
        for (hash = line; (hash = strchr(hash, '#'));) {
            if (hash == line || hash[-1] == ' ' || hash[-1] == '\t') {
                *hash = '\0';
                break;
            }
            hash++;
        }
        eq = strchr(line, '=');

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

const CmColorKey cm_color_keys[] = {
    { "color-capcalera-1", PAL_H1 }, { "color-capcalera-2", PAL_H2 },
    { "color-capcalera-3", PAL_H3 }, { "color-codi", PAL_CODE },
    { "color-cita", PAL_QUOTE },     { "color-error", PAL_ERROR },
    { "color-marcadors", PAL_MARK },
    { "color-text", PAL_TEXT },      { "color-fons", PAL_BG },
    { "color-barra", PAL_BAR },      { "color-barra-fons", PAL_BAR_BG },
    { "color-seleccio", PAL_SEL },   { "color-seleccio-fons", PAL_SEL_BG },
    { "color-menu", PAL_MENU },      { "color-menu-fons", PAL_MENU_BG },
};
const size_t cm_ncolor_keys = sizeof cm_color_keys / sizeof *cm_color_keys;

int cm_rc_color(const char *key, const char *value, short over[PAL_N])
{
    for (size_t i = 0; i < cm_ncolor_keys; i++)
        if (!strcmp(key, cm_color_keys[i].key)) {
            short c = theme_color_parse(value);

            if (c != -2)
                over[cm_color_keys[i].pal] = c;
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

void cm_look_init(CmLook *l)
{
    snprintf(l->tema, sizeof l->tema, "terminal");
    l->console = 1;
    cm_rc_color_reset(l->over);
}

int cm_look_key(CmLook *l, const char *key, const char *value)
{
    if (!strcmp(key, "tema")) {
        snprintf(l->tema, sizeof l->tema, "%s", value);
        cm_rc_color_reset(l->over);     /* un tema nou anul·la els colors d'abans */
        return 1;
    }
    if (!strcmp(key, "paleta-consola")) {
        l->console = !(!strcmp(value, "no") || !strcmp(value, "false") || !strcmp(value, "0"));
        return 1;
    }
    return cm_rc_color(key, value, l->over);
}

int cm_rc_bool(const char *v)
{
    return !strcmp(v, "si") || !strcmp(v, "sí") || !strcmp(v, "yes") || !strcmp(v, "true") ||
           !strcmp(v, "1");
}

void cm_rc_path(const char *v, char *out, size_t size)
{
    const char *home = getenv("HOME");

    if (v[0] == '~' && (v[1] == '/' || !v[1]) && home)
        snprintf(out, size, "%s%s", home, v + 1);
    else
        snprintf(out, size, "%s", v);
}

const char *const cm_shared_keys[CM_NSHARED] = {
    "editor", "textos", "projectes", "recerques",
    "recerca-vault", "recerca-replica", "recerca-entrada",
};

int cm_shared_key(const char *key)
{
    for (int i = 0; i < CM_NSHARED; i++)
        if (!strcmp(key, cm_shared_keys[i]))
            return i;
    return -1;
}

typedef struct {
    const char *key;
    char       *out;
    size_t      size;
    int         found;
} Get;

static void get_cb(const char *k, const char *v, void *ud)
{
    Get *g = ud;

    if (!strcmp(k, g->key)) {
        cm_rc_path(v, g->out, g->size);
        g->found = 1;
    }
}

int cm_rc_get(const char *key, char *out, size_t size)
{
    Get g = { key, out, size, 0 };

    if (size)
        out[0] = '\0';
    cm_rc_read(get_cb, &g);
    return g.found;
}
