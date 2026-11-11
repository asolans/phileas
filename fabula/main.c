#include "fterm.h"
#include "fview.h"
#include "project.h"
#include "flat.h"
#include "version.h"

#include "keys.h"
#include "util.h"

#include <limits.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* fabula --resum CARPETA: el projecte en text, sense pantalla (per a scripts i
 * per mesurar quant triga a llegir-se) */
static int summary(const char *dir)
{
    Project p;
    char err[300], a[32], b[32];
    struct timespec t0, t1;

    clock_gettime(CLOCK_MONOTONIC, &t0);
    if (project_open(&p, dir, err, sizeof err) != 0) {
        fprintf(stderr, "fabula: %s\n", err);
        return 1;
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    fk_fmt_num(project_words(&p), a, sizeof a);
    if (p.objectiu > 0) {
        fk_fmt_num((size_t)p.objectiu, b, sizeof b);
        printf("%s: %s / %s paraules (%ld %%)\n", p.titol, a, b,
               (long)(100.0 * (double)project_words(&p) / (double)p.objectiu + 0.5));
    } else {
        printf("%s: %s paraules\n", p.titol, a);
    }
    for (size_t g = 0; g < p.top->nkids; g++) {
        const FNode *n = p.top->kids[g];

        fk_fmt_num(n->words, a, sizeof a);
        printf("  %s (%zu): %s\n", n->title, n->nkids, a);
        if (n == p.manuscrit)
            for (size_t i = 0; i < n->nkids; i++) {
                fk_fmt_num(n->kids[i]->words, a, sizeof a);
                printf("    %s · %s · %s paraules · %zu escenes\n", n->kids[i]->title,
                       n->kids[i]->estat ? n->kids[i]->estat : "-", a, n->kids[i]->nkids);
            }
    }
    printf("Llegit en %.0f ms\n", (double)(t1.tv_sec - t0.tv_sec) * 1e3 + (double)(t1.tv_nsec - t0.tv_nsec) / 1e6);
    project_free(&p);
    return 0;
}

static void quit_hook(int sig)
{
    ft_end();
    _exit(128 + sig);
}

/* Tema i paleta de la consola, com a Phileas: «tema =» i «paleta-consola =» del
 * phileasrc del sistema i després el de l'usuari. */
static void read_rc(const char *path, char *tema, size_t tsz, int *console)
{
    FILE *f = fopen(path, "r");
    char line[256];

    if (!f)
        return;
    while (fgets(line, sizeof line, f)) {
        char *eq = strchr(line, '='), *k = line, *v;
        size_t l;

        if (line[0] == '#' || !eq)
            continue;
        *eq = '\0';
        v = eq + 1;
        while (*k == ' ' || *k == '\t')
            k++;
        for (l = strlen(k); l && (k[l - 1] == ' ' || k[l - 1] == '\t'); l--)
            k[l - 1] = '\0';
        while (*v == ' ' || *v == '\t')
            v++;
        for (l = strlen(v); l && (v[l - 1] == '\n' || v[l - 1] == '\r' || v[l - 1] == ' '); l--)
            v[l - 1] = '\0';
        if (!strcmp(k, "tema"))
            snprintf(tema, tsz, "%s", v);
        else if (!strcmp(k, "paleta-consola"))
            *console = !(!strcmp(v, "no") || !strcmp(v, "false") || !strcmp(v, "0"));
    }
    fclose(f);
}

int main(int argc, char **argv)
{
    char tema[64] = "terminal", err[300], cfg[512], path[PATH_MAX], rcpath[640];
    int console = 1;
    const Theme *th;
    const char *dir = ".";
    Project p;

    setlocale(LC_ALL, "");
    if (argc > 1 && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help"))) {
        puts("Ús: fabula [CARPETA]\n"
             "     fabula nou \"TÍTOL\" [DIRECTORI]\n"
             "     fabula --resum [CARPETA]    el projecte en text, sense pantalla\n\n"
             "Obre el projecte literari de CARPETA (la que té projecte.md; per defecte, la\n"
             "carpeta actual). «nou» crea la carpeta del projecte (TÍTOL en minúscules i amb\n"
             "guions) dins de DIRECTORI (per defecte, l'actual) i l'obre.\n\n"
             "Retorn obre el capítol o la fitxa a Phileas (o a l'editor de FABULA_EDITOR).\n"
             "El tema es llegeix del phileasrc de Phileas. F1 mostra les tecles.");
        return 0;
    }
    if (argc > 1 && !strcmp(argv[1], "--resum"))
        return summary(argc > 2 ? argv[2] : ".");
    if (argc > 1 && !strcmp(argv[1], "-v")) {
        puts("Fàbula " FABULA_VERSION);
        return 0;
    }
    if (argc > 1 && !strcmp(argv[1], "nou")) {
        char novadir[PATH_MAX];

        if (argc < 3 || argc > 4) {
            fputs("Ús: fabula nou \"TÍTOL\" [DIRECTORI]\n", stderr);
            return 2;
        }
        if (project_create(argc == 4 ? argv[3] : ".", argv[2], novadir, sizeof novadir, err,
                           sizeof err) != 0) {
            fprintf(stderr, "fabula: %s\n", err);
            return 1;
        }
        printf("Projecte creat a %s\n", novadir);
        snprintf(path, sizeof path, "%s", novadir);
        dir = path;
    } else if (argc > 1) {
        dir = argv[1];
    }
    if (project_open(&p, dir, err, sizeof err) != 0) {
        fprintf(stderr, "fabula: %s\n", err);
        return 1;
    }
    read_rc("/etc/phileas/phileasrc", tema, sizeof tema, &console);
    config_dir(cfg, sizeof cfg);
    snprintf(rcpath, sizeof rcpath, "%s/phileasrc", cfg);
    read_rc(rcpath, tema, sizeof tema, &console);
    th = theme_find(tema);
    if (!th)
        th = theme_at(0);

    ft_init(th, console);
    key_quit_hook = quit_hook;
    key_signals();
    fview_run(&p);
    ft_end();
    project_free(&p);
    return 0;
}
