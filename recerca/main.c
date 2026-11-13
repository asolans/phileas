/* Recerca: eina de recerca de terminal, germana de Phileas i Fàbula.
 * Un vault d'Obsidian i un índex SQLite (FTS5) a VAULT/.recerca/. */
#include "fold.h"
#include "fterm.h"
#include "rc.h"
#include "rview.h"
#include "theme.h"
#include "vault.h"
#include "version.h"

#include "keys.h"

#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void ajuda(void)
{
    puts("Recerca " RECERCA_VERSION " — fitxes de recerca en un vault d'Obsidian\n"
         "\n"
         "Ús: recerca [OPCIÓ] [VAULT]\n"
         "\n"
         "VAULT és la carpeta del vault (per defecte, «recerca-vault =» del phileasrc,\n"
         "o si no la carpeta actual). Sense opcions, obre la pantalla de navegació.\n"
         "L'índex és a VAULT/.recerca/index.db: es posa al dia sol cada vegada\n"
         "(només les notes noves o canviades) i es pot esborrar sense perdre res.\n"
         "\n"
         "  --indexa          posa l'índex al dia i diu què ha canviat\n"
         "  --refes           refà l'índex de zero\n"
         "  --resum           fitxes, paraules, tipus, etiquetes i enllaços\n"
         "  --on-surt NOM     notes que enllacen a NOM (backlinks)\n"
         "  --trencats        enllaços que no apunten a cap nota\n"
         "  --cerca TEXT      notes que contenen les paraules (consulta FTS5)\n"
         "  -h, --help        aquesta ajuda\n"
         "  -v, --version     versió\n"
         "\n"
         "A la pantalla, F1 mostra les tecles i Esc el menú. El tema és el de Phileas.\n"
         "«e» edita a Phileas (o a l'editor de RECERCA_EDITOR). Al phileasrc:\n"
         "  recerca-vault = CARPETA     el vault per defecte\n"
         "  recerca-replica = si        el vault és una còpia del PC: només s'escriu\n"
         "  recerca-entrada = _pi       a aquesta carpeta (fitxes noves i anotacions)\n"
         "Els vaults que porta «rep» a la Pi ja són rèpliques (VAULT/.recerca/replica).");
}

static int tty;

static void progres(size_t fet, size_t total, void *ud)
{
    (void)ud;
    if (tty) {
        fprintf(stderr, "\rIndexant %zu/%zu…", fet, total);
        if (fet == total)
            fputs("\r\033[K", stderr);
    }
}

static int sincronitza(Vault *v, int refes, int informa)
{
    VaultSync s;
    char err[512], a[32], b[32], c[32], d[32];

    if (vault_sync(v, refes, progres, NULL, &s, err, sizeof err)) {
        fprintf(stderr, "recerca: %s\n", err);
        return -1;
    }
    if (informa) {
        vault_fmt_num(a, sizeof a, (long long)s.noves);
        vault_fmt_num(b, sizeof b, (long long)s.canviades);
        vault_fmt_num(c, sizeof c, (long long)s.esborrades);
        vault_fmt_num(d, sizeof d, (long long)s.iguals);
        printf("Índex al dia en %.2f s: %s noves, %s canviades, %s esborrades, %s iguals.\n",
               s.segons, a, b, c, d);
    }
    return 0;
}

static long long un(sqlite3 *db, const char *sql)
{
    sqlite3_stmt *s;
    long long n = 0;

    if (sqlite3_prepare_v2(db, sql, -1, &s, NULL) == SQLITE_OK && sqlite3_step(s) == SQLITE_ROW)
        n = sqlite3_column_int64(s, 0);
    sqlite3_finalize(s);
    return n;
}

static void llista(sqlite3 *db, const char *titol, const char *sql, int max)
{
    sqlite3_stmt *s;
    int n = 0;
    char x[32];

    printf("%s", titol);
    if (sqlite3_prepare_v2(db, sql, -1, &s, NULL) != SQLITE_OK)
        return;
    while (sqlite3_step(s) == SQLITE_ROW) {
        if (n == max) {
            printf(", …");
            break;
        }
        vault_fmt_num(x, sizeof x, sqlite3_column_int64(s, 1));
        printf("%s%s %s", n ? ", " : "", sqlite3_column_text(s, 0) ? (const char *)sqlite3_column_text(s, 0) : "(sense)", x);
        n++;
    }
    sqlite3_finalize(s);
    puts(n ? "" : "cap");
}

static int resum(Vault *v)
{
    sqlite3 *db = vault_db(v);
    char a[32], b[32], c[32], d[32], e[32], f[32];
    char path[4096];
    FILE *fp;
    long mida = 0;

    snprintf(path, sizeof path, "%s/%s", vault_root(v), VAULT_DB);
    if ((fp = fopen(path, "rb"))) {
        fseek(fp, 0, SEEK_END);
        mida = ftell(fp);
        fclose(fp);
    }
    printf("Vault: %s (índex de %.1f MB)\n", vault_root(v), mida / 1e6);
    vault_fmt_num(a, sizeof a, un(db, "SELECT count(*) FROM fitxes"));
    vault_fmt_num(b, sizeof b, un(db, "SELECT total(paraules) FROM fitxes"));
    vault_fmt_num(c, sizeof c, un(db, "SELECT count(DISTINCT carpeta) FROM fitxes"));
    printf("%s fitxes · %s paraules · %s carpetes\n", a, b, c);
    llista(db, "Tipus: ", "SELECT tipus, count(*) n FROM fitxes GROUP BY tipus ORDER BY n DESC", 8);
    vault_fmt_num(a, sizeof a, un(db, "SELECT count(DISTINCT etiqueta_min) FROM etiquetes"));
    printf("Etiquetes distintes: %s\n", a);
    llista(db, "  les més usades: ",
           "SELECT etiqueta, count(DISTINCT fitxa) n FROM etiquetes GROUP BY etiqueta_min ORDER BY n DESC", 8);
    vault_fmt_num(a, sizeof a, un(db, "SELECT count(*) FROM enllacos"));
    vault_fmt_num(b, sizeof b, un(db, "SELECT count(*) FROM enllacos WHERE a IS NULL AND NOT adjunt"));
    vault_fmt_num(c, sizeof c, un(db, "SELECT count(*) FROM enllacos WHERE adjunt"));
    vault_fmt_num(d, sizeof d, un(db, "SELECT count(*) FROM enllacos WHERE relacio IS NOT NULL"));
    vault_fmt_num(e, sizeof e, un(db, "SELECT count(*) FROM blocs"));
    vault_fmt_num(f, sizeof f, un(db, "SELECT count(*) FROM alies"));
    printf("Enllaços: %s (trencats %s, a adjunts %s, relacions de propietat %s)\n", a, b, c, d);
    printf("Blocs ^: %s · Àlies: %s\n", e, f);
    return 0;
}

static int on_surt(Vault *v, const char *nom)
{
    sqlite3 *db = vault_db(v);
    sqlite3_stmt *s;
    long long id = 0;
    int n = 0;

    /* la nota com la resoldria un [[NOM]] */
    char *min = cm_lower(nom, strlen(nom));

    sqlite3_prepare_v2(db, "SELECT id, cami FROM fitxes WHERE nom_min = ?1 OR cami_min = ?1 "
                           "OR id = (SELECT fitxa FROM alies WHERE pleg = ?1 LIMIT 1) "
                           "ORDER BY length(cami) LIMIT 1", -1, &s, NULL);
    sqlite3_bind_text(s, 1, min, -1, SQLITE_TRANSIENT);
    free(min);
    if (sqlite3_step(s) == SQLITE_ROW) {
        id = sqlite3_column_int64(s, 0);
        printf("%s\n", sqlite3_column_text(s, 1));
    }
    sqlite3_finalize(s);
    if (!id) {
        fprintf(stderr, "recerca: no hi ha cap nota «%s»\n", nom);
        return 1;
    }
    sqlite3_prepare_v2(db, "SELECT f.cami, e.linia, e.relacio, e.context FROM enllacos e JOIN fitxes f ON f.id = e.de "
                           "WHERE e.a = ? AND e.de <> e.a ORDER BY f.cami, e.linia", -1, &s, NULL);
    sqlite3_bind_int64(s, 1, id);
    while (sqlite3_step(s) == SQLITE_ROW) {
        const char *rel = (const char *)sqlite3_column_text(s, 2);

        if (rel)
            printf("  %s · %s\n", sqlite3_column_text(s, 0), rel);
        else
            printf("  %s:%d: %s\n", sqlite3_column_text(s, 0), sqlite3_column_int(s, 1), sqlite3_column_text(s, 3));
        n++;
    }
    sqlite3_finalize(s);
    printf("%d enllaços\n", n);
    return 0;
}

static int trencats(Vault *v)
{
    sqlite3_stmt *s;
    int n = 0;

    sqlite3_prepare_v2(vault_db(v), "SELECT f.cami, e.linia, e.desti FROM enllacos e JOIN fitxes f ON f.id = e.de "
                                    "WHERE e.a IS NULL AND NOT e.adjunt ORDER BY f.cami, e.linia", -1, &s, NULL);
    while (sqlite3_step(s) == SQLITE_ROW) {
        printf("%s:%d: [[%s]]\n", sqlite3_column_text(s, 0), sqlite3_column_int(s, 1), sqlite3_column_text(s, 2));
        n++;
    }
    sqlite3_finalize(s);
    printf("%d enllaços trencats\n", n);
    return 0;
}

static int cerca(Vault *v, const char *q)
{
    sqlite3_stmt *s;
    int n = 0;

    if (sqlite3_prepare_v2(vault_db(v), "SELECT f.cami FROM text JOIN fitxes f ON f.id = text.rowid "
                                        "WHERE text MATCH ? ORDER BY f.cami", -1, &s, NULL) != SQLITE_OK)
        return 1;
    sqlite3_bind_text(s, 1, q, -1, SQLITE_STATIC);
    while (sqlite3_step(s) == SQLITE_ROW) {
        puts((const char *)sqlite3_column_text(s, 0));
        n++;
    }
    if (sqlite3_errcode(vault_db(v)) != SQLITE_DONE && sqlite3_errcode(vault_db(v)) != SQLITE_OK) {
        fprintf(stderr, "recerca: consulta no vàlida: %s\n", sqlite3_errmsg(vault_db(v)));
        sqlite3_finalize(s);
        return 1;
    }
    sqlite3_finalize(s);
    printf("%d notes\n", n);
    return 0;
}

typedef struct {
    char tema[64];
    int  console;
    char vault[4096];
    int  replica;
    char entrada[256];
} Rc;

static void rc_cb(const char *k, const char *v, void *ud)
{
    Rc *rc = ud;

    if (!strcmp(k, "tema"))
        snprintf(rc->tema, sizeof rc->tema, "%s", v);
    else if (!strcmp(k, "paleta-consola"))
        rc->console = !(!strcmp(v, "no") || !strcmp(v, "false") || !strcmp(v, "0"));
    else if (!strcmp(k, "recerca-vault"))
        snprintf(rc->vault, sizeof rc->vault, "%s", v);
    else if (!strcmp(k, "recerca-replica"))
        rc->replica = !strcmp(v, "si") || !strcmp(v, "sí") || !strcmp(v, "yes") || !strcmp(v, "true") || !strcmp(v, "1");
    else if (!strcmp(k, "recerca-entrada"))
        snprintf(rc->entrada, sizeof rc->entrada, "%s", v);
}

static void quit_hook(int sig)
{
    ft_end();
    _exit(128 + sig);
}

static void progres_pantalla(size_t fet, size_t total, void *ud)
{
    char s[128];

    (void)ud;
    snprintf(s, sizeof s, " Indexant el vault: %zu de %zu fitxes… ", fet, total);
    erase();
    mvaddstr(LINES / 2, COLS > (int)strlen(s) ? (COLS - (int)strlen(s)) / 2 : 0, s);
    refresh();
}

static int pantalla(Vault *v, const Rc *rc)
{
    const Theme *th = theme_find(rc->tema);
    VaultSync s;
    char err[512];

    ft_init(th ? th : theme_at(0), rc->console);
    key_quit_hook = quit_hook;
    key_signals();
    progres_pantalla(0, 0, NULL);
    if (vault_sync(v, 0, progres_pantalla, NULL, &s, err, sizeof err)) {
        ft_end();
        fprintf(stderr, "recerca: %s\n", err);
        return 1;
    }
    {
        RviewOpts o = { rc->replica, rc->entrada[0] ? rc->entrada : "_pi" };
        char path[4200], line[300];
        static char entrada[300];
        FILE *f;

        /* .recerca/replica (el crea rep a la Pi): el vault és una còpia del PC */
        snprintf(path, sizeof path, "%s/" VAULT_DIR "/replica", vault_root(v));
        if ((f = fopen(path, "r"))) {
            o.replica = 1;
            while (fgets(line, sizeof line, f))
                if (!strncmp(line, "entrada=", 8)) {
                    line[strcspn(line, "\r\n")] = '\0';
                    if (line[8]) {
                        snprintf(entrada, sizeof entrada, "%s", line + 8);
                        o.entrada = entrada;
                    }
                }
            fclose(f);
        }
        const char *ed = getenv("RECERCA_EDITOR");

        if (ed && *ed)                      /* launch_editor llegeix FABULA_EDITOR */
            setenv("FABULA_EDITOR", ed, 1);
        rview_run(v, &o);
    }
    ft_end();
    return 0;
}

int main(int argc, char **argv)
{
    const char *op = NULL, *arg = NULL, *dir = NULL;
    Rc rc = { "terminal", 1, "", 0, "" };
    char err[512];
    Vault *v;
    int r = 0;

    setlocale(LC_ALL, "");
    tty = isatty(2);
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];

        if (!strcmp(a, "-h") || !strcmp(a, "--help")) {
            ajuda();
            return 0;
        }
        if (!strcmp(a, "-v") || !strcmp(a, "--version")) {
            printf("Recerca %s (SQLite %s)\n", RECERCA_VERSION, sqlite3_libversion());
            return 0;
        }
        if (!strcmp(a, "--indexa") || !strcmp(a, "--refes") || !strcmp(a, "--resum") || !strcmp(a, "--trencats"))
            op = a;
        else if (!strcmp(a, "--on-surt") || !strcmp(a, "--cerca")) {
            if (i + 1 >= argc) {
                fprintf(stderr, "recerca: %s necessita un argument\n", a);
                return 2;
            }
            op = a;
            arg = argv[++i];
        } else if (a[0] == '-') {
            fprintf(stderr, "recerca: opció desconeguda %s (vegeu --help)\n", a);
            return 2;
        } else
            dir = a;
    }

    cm_rc_read(rc_cb, &rc);
    if (!dir)
        dir = rc.vault[0] ? rc.vault : ".";
    v = vault_open(dir, err, sizeof err);
    if (!v) {
        fprintf(stderr, "recerca: %s\n", err);
        return 1;
    }
    if (!op) {
        if (!isatty(0) || !isatty(1)) {
            fprintf(stderr, "recerca: la pantalla necessita un terminal (vegeu --help)\n");
            r = 1;
        } else
            r = pantalla(v, &rc);
    } else if (!strcmp(op, "--indexa"))
        r = sincronitza(v, 0, 1);
    else if (!strcmp(op, "--refes"))
        r = sincronitza(v, 1, 1);
    else {
        r = sincronitza(v, 0, 0);
        if (!r && !strcmp(op, "--resum"))
            r = resum(v);
        else if (!r && !strcmp(op, "--on-surt"))
            r = on_surt(v, arg);
        else if (!r && !strcmp(op, "--trencats"))
            r = trencats(v);
        else if (!r && !strcmp(op, "--cerca"))
            r = cerca(v, arg);
    }
    vault_close(v);
    return r ? 1 : 0;
}
