#include "rc.h"
#include "fterm.h"
#include "ui.h"
#include "launch.h"
#include "fview.h"
#include "project.h"
#include "compile.h"
#include "diary.h"
#include "find.h"
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

/* fabula --diari [CARPETA]: l'informe del diari d'escriptura, en text */
static int diary_cli(const char *dir)
{
    Project p;
    Diary d;
    char err[300], **l;
    size_t n;

    if (project_open(&p, dir, err, sizeof err) != 0) {
        fprintf(stderr, "fabula: %s\n", err);
        return 1;
    }
    (void)diary_record(p.root, (long)project_words(&p), time(NULL));
    if (diary_load(p.root, &d) != 0) {
        fprintf(stderr, "fabula: no es pot llegir el diari\n");
        project_free(&p);
        return 1;
    }
    l = diary_report(&d, (long)project_words(&p), p.objectiu, p.data_limit, time(NULL), &n);
    for (size_t i = 0; l && i < n; i++) {
        puts(l[i]);
        free(l[i]);
    }
    free(l);
    diary_free(&d);
    project_free(&p);
    return 0;
}

/* fabula --cerca TEXT [CARPETA]: cerca sense majúscules ni accents, en text */
static int search_cli(const char *needle, const char *dir)
{
    Project p;
    FFind f;
    char err[300];
    struct timespec t0, t1;

    if (project_open(&p, dir, err, sizeof err) != 0 || (clock_gettime(CLOCK_MONOTONIC, &t0), 0) ||
        find_in_project(&p, needle, FF_ICASE, &f, err, sizeof err) != 0) {
        fprintf(stderr, "fabula: %s\n", err);
        return 1;
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    for (size_t i = 0; i < f.n && i < 50; i++)
        printf("%s:%zu: %s\n", f.hits[i].file->name, f.hits[i].line + 1, f.hits[i].context);
    if (f.n > 50)
        printf("… (%zu més)\n", f.n - 50);
    printf("%zu coincidències en %zu fitxers (cerca: %.0f ms)\n", f.n, f.files,
           (double)(t1.tv_sec - t0.tv_sec) * 1e3 + (double)(t1.tv_nsec - t0.tv_nsec) / 1e6);
    find_free(&f);
    project_free(&p);
    return f.n ? 0 : 1;
}

/* fabula compila [-o FITXER] [--puja] [--separa-escenes] [--amb-notes] [CARPETA] */
static int compile_cli(int argc, char **argv)
{
    CompileOpts o;
    CompileStats st;
    Project p;
    const char *outpath = NULL, *dir = ".";
    char err[400];
    int rc;

    memset(&o, 0, sizeof o);
    for (int i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "-o") && i + 1 < argc)
            outpath = argv[++i];
        else if (!strcmp(argv[i], "--puja"))
            o.promote = 1;
        else if (!strcmp(argv[i], "--separa-escenes"))
            o.scene_sep = 1;
        else if (!strcmp(argv[i], "--amb-notes"))
            o.keep_notes = 1;
        else if (!strcmp(argv[i], "--amb-enllacos"))
            o.keep_links = 1;
        else if (argv[i][0] == '-' && argv[i][1]) {
            fprintf(stderr, "fabula compila: opció desconeguda: %s\n", argv[i]);
            return 2;
        } else
            dir = argv[i];
    }
    if (project_open(&p, dir, err, sizeof err) != 0) {
        fprintf(stderr, "fabula: %s\n", err);
        return 1;
    }
    if (outpath && strcmp(outpath, "-"))
        rc = project_compile_file(&p, &o, outpath, &st, err, sizeof err);
    else
        rc = project_compile(&p, &o, stdout, &st, err, sizeof err);
    if (rc != 0) {
        fprintf(stderr, "fabula: %s\n", err);
        project_free(&p);
        return 1;
    }
    fprintf(stderr, "Compilat: %zu capítols, %zu paraules%s%s\n", st.chapters, st.words,
            outpath && strcmp(outpath, "-") ? " → " : "", outpath && strcmp(outpath, "-") ? outpath : "");
    if (st.pending)
        fprintf(stderr, "Atenció: al text queden %zu pendents (TODO o [?]).\n", st.pending);
    project_free(&p);
    return 0;
}

static void quit_hook(int sig)
{
    ft_end();
    _exit(128 + sig);
}

/* Tema, paleta i colors del phileasrc (del sistema i de l'usuari), com a
 * Phileas, i l'editor */
static void rc_cb(const char *k, const char *v, void *ud)
{
    if (!cm_look_key(ud, k, v) && !strcmp(k, "editor"))
        launch_set_editor(v);
}

int main(int argc, char **argv)
{
    char err[300], path[PATH_MAX];
    CmLook look;
    const char *dir = ".";
    Project p;

    setlocale(LC_ALL, "");
    if (argc > 1 && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help"))) {
        puts("Ús: fabula [CARPETA]\n"
             "     fabula nou \"TÍTOL\" [DIRECTORI]\n"
             "     fabula --resum [CARPETA]    el projecte en text, sense pantalla\n"
             "     fabula --diari [CARPETA]    el diari d'escriptura: avui, ratxa i previsió\n"
             "     fabula --cerca TEXT [CARPETA]   cerca (sense majúscules ni accents), en text\n"
             "     fabula compila [-o FITXER] [--puja] [--separa-escenes] [--amb-notes] [--amb-enllacos] [CARPETA]\n"
             "                     el manuscrit en un sol document (.md, o .docx/.epub/.pdf amb pandoc)\n\n"
             "Obre el projecte literari de CARPETA (la que té projecte.md; per defecte, la\n"
             "carpeta actual). «nou» crea la carpeta del projecte (TÍTOL en minúscules i amb\n"
             "guions) dins de DIRECTORI (per defecte, l'actual) i l'obre.\n\n"
             "Retorn obre el capítol o la fitxa a Phileas (o a l'editor de «editor =» del\n"
             "phileasrc, o de FABULA_EDITOR, que hi mana).\n"
             "«b» obre Recerca amb el vault de «recerca:» de projecte.md; en sortir, torna.\n"
             "El tema i els colors es llegeixen del phileasrc, com a Phileas. F1 mostra les tecles.");
        return 0;
    }
    if (argc > 1 && !strcmp(argv[1], "compila"))
        return compile_cli(argc - 2, argv + 2);
    if (argc > 2 && !strcmp(argv[1], "--cerca"))
        return search_cli(argv[2], argc > 3 ? argv[3] : ".");
    if (argc > 1 && !strcmp(argv[1], "--diari"))
        return diary_cli(argc > 2 ? argv[2] : ".");
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
    cm_look_init(&look);
    cm_rc_read(rc_cb, &look);
    cm_look_start(&look);
    key_quit_hook = quit_hook;
    key_signals();
    fview_run(&p);
    ft_end();
    project_free(&p);
    return 0;
}
