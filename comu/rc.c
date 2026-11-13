#include "rc.h"

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
