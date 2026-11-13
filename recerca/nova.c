#include "nova.h"

#include "yaml.h"
#include "util.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

char *nova_nom(const char *titol)
{
    size_t n = strlen(titol), o = 0;
    char *out = xmalloc(n + 1);

    for (size_t i = 0; i < n; i++) {
        char c = titol[i];

        if (strchr("*\"\\/<>:|?#^[]", c) || (unsigned char)c < 32)
            continue;
        out[o++] = c;
    }
    out[o] = '\0';
    while (o && (out[o - 1] == ' ' || out[o - 1] == '.'))     /* ni espais ni punts al final */
        out[--o] = '\0';
    {
        size_t a = 0;

        while (out[a] == ' ' || out[a] == '.')
            a++;
        memmove(out, out + a, o - a + 1);
    }
    return out;
}

void nova_carpeta_plantilles(const char *root, char *out, size_t outsz)
{
    char path[4096], buf[4096];
    FILE *f;
    size_t n;
    const char *k, *a, *b;

    snprintf(out, outsz, "plantilles");
    snprintf(path, sizeof path, "%s/.obsidian/templates.json", root);
    f = fopen(path, "rb");
    if (!f)
        return;
    n = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    buf[n] = '\0';
    k = strstr(buf, "\"folder\"");
    if (!k || !(a = strchr(k + 8, ':')) || !(a = strchr(a, '"')) || !(b = strchr(a + 1, '"')))
        return;
    if (b > a + 1)
        snprintf(out, outsz, "%.*s", (int)(b - a - 1), a + 1);
}

size_t nova_plantilles(const char *root, char ***out)
{
    char dir[600], path[4096];
    DIR *d;
    struct dirent *e;
    size_t n = 0;

    *out = NULL;
    nova_carpeta_plantilles(root, dir, sizeof dir);
    snprintf(path, sizeof path, "%s/%s", root, dir);
    d = opendir(path);
    if (!d)
        return 0;
    while ((e = readdir(d))) {
        size_t l = strlen(e->d_name);
        char rel[1200];

        if (e->d_name[0] == '.' || l < 4 || strcmp(e->d_name + l - 3, ".md"))
            continue;
        snprintf(rel, sizeof rel, "%s/%s", dir, e->d_name);
        *out = xrealloc(*out, (n + 1) * sizeof **out);
        (*out)[n++] = xstrdup(rel);
    }
    closedir(d);
    return n;
}

void nova_llista_free(char **l, size_t n)
{
    for (size_t i = 0; i < n; i++)
        free(l[i]);
    free(l);
}

/* Substitueix totes les aparicions de from per to (cadena nova) */
static char *replace(const char *s, const char *from, const char *to)
{
    size_t fl = strlen(from), tl = strlen(to), n = 0, o = 0;
    const char *p;
    char *out;

    for (p = s; (p = strstr(p, from)); p += fl)
        n++;
    out = xmalloc(strlen(s) + n * (tl > fl ? tl - fl : 0) + 1);
    for (p = s;;) {
        const char *q = strstr(p, from);

        if (!q) {
            strcpy(out + o, p);
            break;
        }
        memcpy(out + o, p, (size_t)(q - p));
        o += (size_t)(q - p);
        memcpy(out + o, to, tl);
        o += tl;
        p = q + fl;
    }
    return out;
}

char *nova_text(const char *plantilla, const char *titol, const char *id, const char *ara,
                const char *anota)
{
    char data[16], hora[8], *t, *u;
    size_t len;

    snprintf(data, sizeof data, "%.10s", ara);
    snprintf(hora, sizeof hora, "%.5s", strlen(ara) > 11 ? ara + 11 : "");
    if (plantilla) {
        static const char *const keys[] = { "{{title}}", "{{titol}}", "{{date}}", "{{data}}", "{{time}}", "{{hora}}" };
        const char *vals[] = { titol, titol, data, data, hora, hora };

        t = xstrdup(plantilla);
        for (size_t i = 0; i < sizeof keys / sizeof *keys; i++) {
            u = replace(t, keys[i], vals[i]);
            free(t);
            t = u;
        }
    } else {
        size_t n = strlen(titol) + 8;

        t = xmalloc(n);
        snprintf(t, n, "# %s\n\n", titol);
    }
    /* capçalera: id, created i anota (yaml_set la crea si no n'hi ha) */
    u = yaml_set(t, strlen(t), "id", id, 0, &len);
    free(t);
    t = u;
    u = yaml_set(t, len, "created", ara, 0, &len);
    free(t);
    t = u;
    if (anota) {
        size_t n = strlen(anota) + 8;
        char *v = xmalloc(n);

        snprintf(v, n, "\"[[%s]]\"", anota);
        u = yaml_set(t, len, "anota", v, 1, &len);
        free(v);
        free(t);
        t = u;
    }
    return t;
}

void nova_id(const char *prefix, char *out, size_t outsz)
{
    time_t now = time(NULL);
    struct tm tm;

    localtime_r(&now, &tm);
    snprintf(out, outsz, "%s%04d%02d%02d-%02d%02d%02d", prefix, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
             tm.tm_hour, tm.tm_min, tm.tm_sec);
}

static int mkdirs(const char *path)
{
    char tmp[4096];

    snprintf(tmp, sizeof tmp, "%s", path);
    for (char *p = tmp + 1; *p; p++)
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, 0755) && errno != EEXIST)
                return -1;
            *p = '/';
        }
    return mkdir(tmp, 0755) && errno != EEXIST ? -1 : 0;
}

int nova_crea(const char *root, const char *carpeta, const char *nom, const char *text,
              char *rel, size_t relsz, char *err, size_t errsz)
{
    char dir[4096], path[8192];
    size_t len = strlen(text);
    int fd = -1;

    if (!*nom) {
        snprintf(err, errsz, "el títol no pot quedar buit");
        return -1;
    }
    snprintf(dir, sizeof dir, "%s%s%s", root, *carpeta ? "/" : "", carpeta);
    if (mkdirs(dir)) {
        snprintf(err, errsz, "no es pot crear la carpeta %s: %s", carpeta, strerror(errno));
        return -1;
    }
    for (int k = 1; k < 1000 && fd < 0; k++) {
        if (k == 1)
            snprintf(rel, relsz, "%s%s%s.md", carpeta, *carpeta ? "/" : "", nom);
        else
            snprintf(rel, relsz, "%s%s%s %d.md", carpeta, *carpeta ? "/" : "", nom, k);
        snprintf(path, sizeof path, "%s/%s", root, rel);
        fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0644);
        if (fd < 0 && errno != EEXIST) {
            snprintf(err, errsz, "no es pot crear %s: %s", rel, strerror(errno));
            return -1;
        }
    }
    if (fd < 0) {
        snprintf(err, errsz, "massa fitxers amb el nom «%s»", nom);
        return -1;
    }
    if (write(fd, text, len) != (ssize_t)len || fsync(fd) || close(fd)) {
        snprintf(err, errsz, "no es pot escriure %s: %s", rel, strerror(errno));
        return -1;
    }
    return 0;
}

int nova_paperera(const char *root, const char *rel, char *dest, size_t destsz, char *err, size_t errsz)
{
    char from[8192], to[8192], dir[4096];
    const char *base = strrchr(rel, '/');
    struct stat st;

    base = base ? base + 1 : rel;
    snprintf(dir, sizeof dir, "%s/.trash", root);
    if (mkdir(dir, 0755) && errno != EEXIST) {
        snprintf(err, errsz, "no es pot crear .trash: %s", strerror(errno));
        return -1;
    }
    snprintf(from, sizeof from, "%s/%s", root, rel);
    for (int k = 1; k < 1000; k++) {
        size_t bl = strlen(base);

        if (k == 1)
            snprintf(dest, destsz, ".trash/%s", base);
        else
            snprintf(dest, destsz, ".trash/%.*s %d.md", (int)(bl > 3 ? bl - 3 : bl), base, k);
        snprintf(to, sizeof to, "%s/%s", root, dest);
        if (lstat(to, &st) && errno == ENOENT) {
            if (rename(from, to)) {
                snprintf(err, errsz, "no es pot moure a la paperera: %s", strerror(errno));
                return -1;
            }
            return 0;
        }
    }
    snprintf(err, errsz, "la paperera ja té massa fitxers amb aquest nom");
    return -1;
}
