/* Proves de Recerca (recerca/ i comu/): make test-recerca */
#include "fold.h"
#include "front.h"
#include "mdscan.h"
#include "model.h"
#include "nova.h"
#include "rc.h"
#include "vault.h"

#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int failures, checks;

#define CHECK(cond)                                                     \
    do {                                                                \
        checks++;                                                       \
        if (!(cond)) {                                                  \
            failures++;                                                 \
            fprintf(stderr, "%s:%d: falla: %s\n", __FILE__, __LINE__, #cond); \
        }                                                               \
    } while (0)

static char tmpdir[] = "/tmp/recerca-test-XXXXXX";

static void put(const char *rel, const char *text)
{
    char path[512];
    FILE *f;

    snprintf(path, sizeof path, "%s/%s", tmpdir, rel);
    for (char *s = path + strlen(tmpdir) + 1; *s; s++)
        if (*s == '/') {
            *s = '\0';
            mkdir(path, 0755);
            *s = '/';
        }
    f = fopen(path, "wb");
    fputs(text, f);
    fclose(f);
}

static void rm(const char *rel)
{
    char path[512];

    snprintf(path, sizeof path, "%s/%s", tmpdir, rel);
    unlink(path);
}

/* ---------- fold ---------- */

static int eqfree(char *s, const char *want)
{
    int r = !strcmp(s, want);

    if (!r)
        fprintf(stderr, "  obtingut «%s», esperat «%s»\n", s, want);
    free(s);
    return r;
}

static void test_fold(void)
{
    CHECK(eqfree(cm_lower("ÀÉÏÒÚ Ç Ñ Abc", strlen("ÀÉÏÒÚ Ç Ñ Abc")), "àéïòú ç ñ abc"));
    CHECK(eqfree(cm_fold("ÀÉÏÒÚ Ç Ñ Abc", strlen("ÀÉÏÒÚ Ç Ñ Abc")), "aeiou c n abc"));
    CHECK(eqfree(cm_fold("Col·lecció", strlen("Col·lecció")), "colleccio"));
    CHECK(eqfree(cm_fold("Œuvre Straße", strlen("Œuvre Straße")), "oeuvre strasse"));
    CHECK(eqfree(cm_lower("Història", strlen("Història")), "història"));
    CHECK(eqfree(cm_fold("a\xff" "b", 3), "a\xff" "b"));     /* byte invàlid: tal qual */
    /* per caràcters (la cerca de Fàbula) */
    CHECK(cm_lower_char(L'À') == L'à' && cm_lower_char(L'Q') == L'q');
    CHECK(!strcmp(cm_strip_char(L'à'), "a") && !strcmp(cm_strip_char(L'æ'), "ae"));
    CHECK(!strcmp(cm_strip_char(0xB7), "") && cm_strip_char(L'x') == NULL);
}

/* ---------- rc (el phileasrc comú) ---------- */

static void test_rc(void)
{
    CmLook l;
    char path[64];

    cm_look_init(&l);
    CHECK(!strcmp(l.tema, "terminal") && l.console == 1 && l.over[PAL_BAR] == CM_NO_COLOR);
    CHECK(cm_look_key(&l, "color-barra", "vermell") && l.over[PAL_BAR] == 1);
    CHECK(cm_look_key(&l, "tema", "sepia") && !strcmp(l.tema, "sepia"));
    CHECK(l.over[PAL_BAR] == CM_NO_COLOR);       /* el tema anul·la els colors d'abans */
    CHECK(cm_look_key(&l, "paleta-consola", "no") && l.console == 0);
    CHECK(!cm_look_key(&l, "recerca-vault", "/x") && !cm_look_key(&l, "columnes", "60"));
    CHECK(cm_rc_bool("sí") && cm_rc_bool("si") && cm_rc_bool("1") && !cm_rc_bool("no"));
    {
        char *home = getenv("HOME") ? strdup(getenv("HOME")) : NULL;

        setenv("HOME", "/casa", 1);
        cm_rc_path("~/tesi", path, sizeof path);
        CHECK(!strcmp(path, "/casa/tesi"));
        if (home)
            setenv("HOME", home, 1);
        free(home);
    }
    cm_rc_path("/data/recerca", path, sizeof path);
    CHECK(!strcmp(path, "/data/recerca"));
    CHECK(cm_ncolor_keys == 15 && !strcmp(cm_color_keys[0].key, "color-capcalera-1"));
    CHECK(cm_shared_key("projectes") == CM_K_PROJECTES && cm_shared_key("columnes") < 0);

    /* cm_rc_get: el phileasrc de l'usuari, amb els comentaris com a Phileas */
    {
        char dir[600], rc[700], val[256], *old = getenv("PHILEAS_CONFIG") ? strdup(getenv("PHILEAS_CONFIG")) : NULL;
        FILE *f;

        snprintf(dir, sizeof dir, "%s/rc", tmpdir);
        mkdir(dir, 0755);
        snprintf(rc, sizeof rc, "%s/phileasrc", dir);
        f = fopen(rc, "w");
        fputs("# prova-x = no\nprova-x = u   # comentari\nprova-y = a#b\nprova-x = dos\n", f);
        fclose(f);
        setenv("PHILEAS_CONFIG", dir, 1);
        CHECK(cm_rc_get("prova-x", val, sizeof val) && !strcmp(val, "dos"));
        CHECK(cm_rc_get("prova-y", val, sizeof val) && !strcmp(val, "a#b"));
        CHECK(!cm_rc_get("prova-z", val, sizeof val) && !val[0]);
        if (old)
            setenv("PHILEAS_CONFIG", old, 1);
        else
            unsetenv("PHILEAS_CONFIG");
        free(old);
    }
}

/* ---------- front ---------- */

typedef struct {
    char k[16][32], v[16][64];
    int idx[16], n;
} Got;

static void got_cb(const char *k, size_t kl, const char *v, size_t vl, int idx, void *ud)
{
    Got *g = ud;

    if (g->n >= 16)
        return;
    snprintf(g->k[g->n], 32, "%.*s", (int)kl, k);
    snprintf(g->v[g->n], 64, "%.*s", (int)vl, v);
    g->idx[g->n++] = idx;
}

static void test_front(void)
{
    const char *t =
        "---\n"
        "tipus: fons\n"
        "data: \"1348?\"\n"
        "arxiu: 'ACA' # comentari\n"
        "tags: [fiscalitat, \"segle xiv\", tema/pesta]\n"
        "aliases:\n"
        "  - Pere III\n"
        "  - \"el Cerimoniós\"\n"
        "contradiu: \"[[Font B]]\"\n"
        "nested:\n"
        "  clau: valor\n"
        "---\n"
        "# Títol\n";
    Got g;
    size_t line, off;

    memset(&g, 0, sizeof g);
    off = cm_front(t, strlen(t), &line, got_cb, &g);
    CHECK(off > 0 && !strncmp(t + off, "# Títol", 7));
    CHECK(line == 13);
    CHECK(g.n == 9);
    CHECK(!strcmp(g.k[0], "tipus") && !strcmp(g.v[0], "fons") && g.idx[0] == -1);
    CHECK(!strcmp(g.v[1], "1348?"));
    CHECK(!strcmp(g.v[2], "ACA"));
    CHECK(!strcmp(g.k[3], "tags") && !strcmp(g.v[3], "fiscalitat") && g.idx[3] == 0);
    CHECK(!strcmp(g.v[4], "segle xiv") && g.idx[4] == 1);
    CHECK(!strcmp(g.v[5], "tema/pesta") && g.idx[5] == 2);
    CHECK(!strcmp(g.k[6], "aliases") && !strcmp(g.v[6], "Pere III") && g.idx[6] == 0);
    CHECK(!strcmp(g.v[7], "el Cerimoniós") && g.idx[7] == 1);
    CHECK(!strcmp(g.k[8], "contradiu") && !strcmp(g.v[8], "[[Font B]]"));

    memset(&g, 0, sizeof g);
    CHECK(cm_front("# Sense capçalera\n", 18, &line, got_cb, &g) == 0 && line == 1 && g.n == 0);
    CHECK(cm_front("---\nclau: x\nsense tancar\n", 26, &line, NULL, NULL) == 0);
    memset(&g, 0, sizeof g);
    t = "---\r\nclau: \"a, b\"\r\nl: [\"[[A]]\", \"[[B|b]]\"]\r\n---\r\ncos";
    off = cm_front(t, strlen(t), &line, got_cb, &g);
    CHECK(off && !strcmp(t + off, "cos"));
    CHECK(g.n == 3 && !strcmp(g.v[0], "a, b") && !strcmp(g.v[1], "[[A]]") && !strcmp(g.v[2], "[[B|b]]"));
}

/* ---------- mdscan ---------- */

typedef struct {
    int kind[32];
    char t[32][64], sec[32][32], al[32][32];
    int block[32], embed[32];
    size_t line[32], start[32], len[32];
    int n;
} Refs;

static void ref_cb(const CmRef *r, void *ud)
{
    Refs *x = ud;

    if (x->n >= 32)
        return;
    x->kind[x->n] = r->kind;
    snprintf(x->t[x->n], 64, "%.*s", (int)r->target_len, r->target);
    snprintf(x->sec[x->n], 32, "%.*s", (int)r->section_len, r->section ? r->section : "");
    snprintf(x->al[x->n], 32, "%.*s", (int)r->alias_len, r->alias ? r->alias : "");
    x->block[x->n] = r->block;
    x->embed[x->n] = r->embed;
    x->line[x->n] = r->line;
    x->start[x->n] = r->start;
    x->len[x->n] = r->len;
    x->n++;
}

static void test_scan(void)
{
    const char *t =
        "# Capçalera no és etiqueta\n"                                  /* 10 */
        "Text [[Nota]] i [[Carpeta/Altra|amb text]] i [[N#Secció]].\n"  /* 11 */
        "Bloc [[N#^b1]], incrustat ![[Imatge.png]] i [x](Dir/Nom%20llarg.md#s).\n"
        "Etiquetes #fiscalitat, #tema/pesta i #segle_xiv; no #123 ni a#b ni ##x.\n"
        "Paràgraf amb bloc ^id-1\n"
        "`codi [[No]] #no` fora [[Sí]]\n"
        "```\n"
        "[[Dins del codi]] #codi\n"
        "```\n"
        "%% comentari [[No2]]\n"
        "segueix %% després [[Sí2]]\n"
        "<!-- [[No3]] --> [[Sí3]] [web](https://x.org/a.md)\n"
        "| [[Taula\\|alies]] | #català·la |\n";
    Refs x;

    memset(&x, 0, sizeof x);
    cm_scan(t, strlen(t), 10, ref_cb, &x);
    for (int i = 0; i < x.n; i++)
        if (getenv("RV"))
            fprintf(stderr, "  %d: %d «%s»\n", i, x.kind[i], x.t[i]);
    CHECK(x.n == 15);
    CHECK(x.kind[0] == CM_LINK && !strcmp(x.t[0], "Nota") && x.line[0] == 11);
    CHECK(x.start[0] == 5 && x.len[0] == 8);           /* «[[Nota]]» */
    CHECK(x.start[4] == strlen("Bloc [[N#^b1]], incrustat ") && x.len[4] == strlen("![[Imatge.png]]"));
    CHECK(!strcmp(x.t[1], "Carpeta/Altra") && !strcmp(x.al[1], "amb text"));
    CHECK(!strcmp(x.t[2], "N") && !strcmp(x.sec[2], "Secció") && !x.block[2]);
    CHECK(!strcmp(x.t[3], "N") && !strcmp(x.sec[3], "b1") && x.block[3]);
    CHECK(!strcmp(x.t[4], "Imatge.png") && x.embed[4]);
    CHECK(!strcmp(x.t[5], "Dir/Nom llarg") && !strcmp(x.sec[5], "s") && !strcmp(x.al[5], "x"));
    CHECK(x.kind[6] == CM_TAG && !strcmp(x.t[6], "fiscalitat"));
    CHECK(!strcmp(x.t[7], "tema/pesta") && !strcmp(x.t[8], "segle_xiv"));
    CHECK(x.kind[9] == CM_BLOCK && !strcmp(x.t[9], "id-1") && x.line[9] == 14);
    CHECK(!strcmp(x.t[10], "Sí"));
    CHECK(!strcmp(x.t[11], "Sí2"));
    CHECK(!strcmp(x.t[12], "Sí3"));
    CHECK(!strcmp(x.t[13], "Taula") && !strcmp(x.al[13], "alies"));
    CHECK(x.kind[14] == CM_TAG && !strcmp(x.t[14], "català·la"));
    CHECK(x.n < 16 || x.kind[15] != CM_LINK || strcmp(x.t[15], "https://x.org/a"));

    CHECK(cm_tag_valid("segle_xiv", 9) && !cm_tag_valid("1348", 4) && cm_tag_valid("1348a", 5));
    CHECK(!cm_tag_valid("a«b", strlen("a«b")));

    memset(&x, 0, sizeof x);
    cm_scan_value("[[A]], [[B|b]]", 14, ref_cb, &x);
    CHECK(x.n == 2 && !strcmp(x.t[0], "A") && !strcmp(x.t[1], "B"));
}

/* ---------- vault ---------- */

static long long q1(Vault *v, const char *sql)
{
    sqlite3_stmt *s;
    long long n = -1;

    if (sqlite3_prepare_v2(vault_db(v), sql, -1, &s, NULL) == SQLITE_OK && sqlite3_step(s) == SQLITE_ROW)
        n = sqlite3_column_int64(s, 0);
    else
        fprintf(stderr, "  SQL: %s: %s\n", sql, sqlite3_errmsg(vault_db(v)));
    sqlite3_finalize(s);
    return n;
}

static int sinc(Vault *v, VaultSync *s)
{
    char err[256];
    int r = vault_sync(v, 0, NULL, NULL, s, err, sizeof err);

    if (r)
        fprintf(stderr, "  sync: %s\n", err);
    return r;
}

static void test_vault(void)
{
    char err[256], sql[512];
    Vault *v;
    VaultSync s;

    put("Persones/Pere el Cerimoniós.md",
        "---\nid: p1\ntipus: persona\ndata: 1319/1387\naliases:\n  - Pere III\ntags:\n  - corona\n---\n"
        "# Pere\n\nRei. Vegeu [[Corts de Montsó]] i [[Corts]].\n");
    put("Fons/ACA/Corts de Montsó.md",
        "---\ntipus: fons\ndata: \"1363\"\ntags: [corts, fiscalitat]\ncomplementa: \"[[Pere III]]\"\n---\n"
        "Text sobre la història fiscal. [[Pere III]] hi va ser. ^b1\n"
        "Una col·lecció de documents #Fiscalitat. [[#Secció]] [[Mapa.png]]\n");
    put("Corts.md", "Nota curta a l'arrel. [[Fons/ACA/Corts de Montsó#^b1]] [[No existeix]]\n");
    put("Temes/Corts.md", "Una altra nota amb el mateix nom, més llarga de camí.\n");
    put(".obsidian/workspace.md", "[[No s'ha de llegir]]\n");
    put("Notes/sense.txt", "no és markdown\n");

    v = vault_open(tmpdir, err, sizeof err);
    CHECK(v != NULL);
    if (!v) {
        fprintf(stderr, "  %s\n", err);
        return;
    }
    CHECK(sinc(v, &s) == 0);
    CHECK(s.noves == 4 && s.canviades == 0 && s.esborrades == 0);
    CHECK(q1(v, "SELECT count(*) FROM fitxes") == 4);
    CHECK(q1(v, "SELECT count(*) FROM fitxes WHERE tipus='persona' AND data='1319/1387' AND uid='p1'") == 1);
    CHECK(q1(v, "SELECT count(*) FROM fitxes WHERE carpeta='Fons/ACA' AND nom='Corts de Montsó'") == 1);
    /* etiquetes: de la capçalera (llista en bloc i en línia) i del text, sense majúscules */
    CHECK(q1(v, "SELECT count(DISTINCT etiqueta_min) FROM etiquetes") == 3);
    CHECK(q1(v, "SELECT count(*) FROM etiquetes WHERE etiqueta_min='fiscalitat'") == 2);
    CHECK(q1(v, "SELECT count(*) FROM etiquetes WHERE etiqueta='Fiscalitat' AND linia=8") == 1);
    /* resolució */
    CHECK(q1(v, "SELECT count(*) FROM enllacos e JOIN fitxes f ON f.id=e.a WHERE e.desti='Corts' AND f.cami='Corts.md'") == 1);
    CHECK(q1(v, "SELECT count(*) FROM enllacos e JOIN fitxes f ON f.id=e.a WHERE e.desti='Pere III' "
                "AND f.nom='Pere el Cerimoniós'") == 2);
    CHECK(q1(v, "SELECT count(*) FROM enllacos WHERE relacio='complementa' AND a IS NOT NULL") == 1);
    CHECK(q1(v, "SELECT count(*) FROM enllacos e JOIN fitxes f ON f.id=e.a WHERE e.bloc=1 AND e.seccio='b1' "
                "AND f.nom='Corts de Montsó'") == 1);
    CHECK(q1(v, "SELECT count(*) FROM enllacos WHERE pleg='' AND a=de") == 1);
    CHECK(q1(v, "SELECT count(*) FROM enllacos WHERE adjunt AND a IS NULL") == 1);
    CHECK(q1(v, "SELECT count(*) FROM enllacos WHERE a IS NULL AND NOT adjunt") == 1);
    CHECK(q1(v, "SELECT count(*) FROM enllacos WHERE desti LIKE 'No s%'") == 0);
    CHECK(q1(v, "SELECT count(*) FROM blocs WHERE bloc='b1' AND linia=7") == 1);
    CHECK(q1(v, "SELECT count(*) FROM propietats WHERE clau='tags'") == 3);
    /* text complet: sense accents ni punt volat */
    CHECK(q1(v, "SELECT count(*) FROM text WHERE text MATCH 'historia'") == 1);
    CHECK(q1(v, "SELECT count(*) FROM text WHERE text MATCH 'collecció'") == 1);
    CHECK(q1(v, "SELECT count(*) FROM text WHERE text MATCH 'montso'") == 3);      /* nom i enllaços */
    CHECK(q1(v, "SELECT count(*) FROM text WHERE text MATCH 'fons'") == 2);        /* propietat i camí */

    /* res no canvia */
    CHECK(sinc(v, &s) == 0 && s.iguals == 4 && !s.noves && !s.canviades && !s.esborrades);

    /* canvis: una nota nova resol l'enllaç trencat; una d'esborrada el trenca */
    put("Notes/No existeix.md", "Ara sí.\n");
    rm("Temes/Corts.md");
    put("Corts.md", "Nota curta canviada, ara sense enllaços.\n");
    CHECK(sinc(v, &s) == 0 && s.noves == 1 && s.canviades == 1 && s.esborrades == 1 && s.iguals == 2);
    CHECK(q1(v, "SELECT count(*) FROM fitxes") == 4);
    CHECK(q1(v, "SELECT count(*) FROM enllacos WHERE a IS NULL AND NOT adjunt") == 0);
    CHECK(q1(v, "SELECT count(*) FROM enllacos WHERE desti='No existeix'") == 0);
    CHECK(q1(v, "SELECT count(*) FROM text WHERE text MATCH 'canviada'") == 1);
    CHECK(q1(v, "SELECT count(*) FROM text WHERE text MATCH 'llarga'") == 0);

    /* reindexar una sola nota */
    put("Notes/No existeix.md", "Ara enllaça a [[pere iii]] (sense majúscules).\n");
    CHECK(vault_reindex(v, "Notes/No existeix.md", err, sizeof err) == 0);
    snprintf(sql, sizeof sql, "SELECT count(*) FROM enllacos e JOIN fitxes f ON f.id=e.a "
                              "WHERE e.desti='pere iii' AND f.uid='p1'");
    CHECK(q1(v, sql) == 1);
    rm("Notes/No existeix.md");
    CHECK(vault_reindex(v, "Notes/No existeix.md", err, sizeof err) == 0);
    CHECK(q1(v, "SELECT count(*) FROM fitxes") == 3);
    CHECK(q1(v, "SELECT count(*) FROM text WHERE text MATCH 'majuscules'") == 0);

    /* resolució parcial: treure un àlies trenca els enllaços que l'usaven */
    put("Persones/Pere el Cerimoniós.md", "---\nid: p1\n---\nSense àlies.\n");
    CHECK(sinc(v, &s) == 0 && s.canviades == 1);
    CHECK(q1(v, "SELECT count(*) FROM enllacos WHERE pleg='pere iii' AND a IS NULL") == 2);
    put("Persones/Pere el Cerimoniós.md", "---\nid: p1\naliases: [Pere III]\n---\nAmb àlies.\n");
    CHECK(sinc(v, &s) == 0 && s.canviades == 1);
    CHECK(q1(v, "SELECT count(*) FROM enllacos WHERE pleg='pere iii' AND a IS NULL") == 0);

    /* resolució d'un sol enllaç i model de la pantalla */
    CHECK(vault_resolve(v, NULL, "Pere III", 8) == q1(v, "SELECT id FROM fitxes WHERE uid='p1'"));
    CHECK(vault_resolve(v, NULL, "fons/aca/corts de montsó", strlen("fons/aca/corts de montsó")) > 0);
    CHECK(vault_resolve(v, NULL, "Mapa.png", 8) == -1);
    CHECK(vault_resolve(v, NULL, "Res", 3) == 0);
    {
        Model m;
        size_t *l, n;

        memset(&m, 0, sizeof m);
        CHECK(model_load(&m, vault_db(v)) == 0 && m.n == 3);
        /* carpetes: Totes, Fons, Fons/ACA, Persones, (a l'arrel) */
        CHECK(m.nnav == 5 && !m.nav[0].key && m.nav[0].count == 3);
        CHECK(!strcmp(m.nav[1].key, "Fons") && m.nav[1].count == 1 && m.nav[1].depth == 1);
        CHECK(!strcmp(m.nav[2].key, "Fons/ACA") && m.nav[2].depth == 2 && !strcmp(m.nav[2].label, "ACA"));
        CHECK(!strcmp(m.nav[4].key, "") && m.nav[4].count == 1);
        n = model_list(&m, &m.nav[4], "", SORT_NOM, &l);
        CHECK(n == 1 && !strcmp(m.notes[l[0]].cami, "Corts.md"));
        free(l);
        n = model_list(&m, NULL, "montso", SORT_NOM, &l);      /* filtre sense accents */
        CHECK(n == 1 && !strcmp(m.notes[l[0]].nom, "Corts de Montsó"));
        free(l);
        n = model_list(&m, NULL, "", SORT_DATA, &l);           /* sense data, al final */
        CHECK(n == 3 && !strcmp(m.notes[l[0]].data, "1363") && !m.notes[l[2]].data[0]);
        free(l);
        model_set_mode(&m, NAV_ETIQUETES);
        CHECK(m.nnav == 3 && !strcmp(m.nav[1].key, "corts") && !strcmp(m.nav[2].key, "fiscalitat"));
        n = model_list(&m, &m.nav[2], "", SORT_NOM, &l);
        CHECK(n == 1);
        free(l);
        model_set_mode(&m, NAV_TIPUS);
        CHECK(m.nnav == 3 && !strcmp(m.nav[1].key, "fons") && !strcmp(m.nav[2].label, "(sense tipus)"));
        CHECK(model_type_letter("nota de camp") == 'N' && model_type_letter("fons") == 'F' && model_type_letter("") == ' ');
        CHECK(model_find(&m, m.notes[1].id) == 1 && model_find(&m, 999999) == -1);
        model_free(&m);
    }

    /* refer de zero */
    CHECK(vault_sync(v, 1, NULL, NULL, &s, err, sizeof err) == 0 && s.noves == 3);

    /* un esquema d'una altra versió es buida en obrir */
    sqlite3_exec(vault_db(v), "UPDATE meta SET valor='0' WHERE clau='esquema'", NULL, NULL, NULL);
    vault_close(v);
    v = vault_open(tmpdir, err, sizeof err);
    CHECK(v && q1(v, "SELECT count(*) FROM fitxes") == 0);
    CHECK(v && sinc(v, &s) == 0 && s.noves == 3);
    vault_close(v);

    CHECK(vault_open("/no/existeix", err, sizeof err) == NULL);
}

/* Enllaços Markdown relatius a la carpeta de la nota (en un vault a part) */
static void test_relatiu(void)
{
    char root[600], err[256];
    Vault *v;
    VaultSync s;

    put("rel/Persones/Pere el Cerimoniós.md", "Rei.\n");
    put("rel/Fons/ACA/Corts de Montsó.md", "Corts.\n");
    put("rel/Fons/ACA/Relatiu.md", "[a](../../Persones/Pere%20el%20Cerimoniós.md) [b](./Corts%20de%20Montsó.md) "
                                   "[c](../Res.md)\n");
    snprintf(root, sizeof root, "%s/rel", tmpdir);
    v = vault_open(root, err, sizeof err);
    CHECK(v && sinc(v, &s) == 0 && s.noves == 3);
    if (!v)
        return;
    CHECK(q1(v, "SELECT count(*) FROM enllacos WHERE a IS NOT NULL") == 2);
    CHECK(q1(v, "SELECT count(*) FROM enllacos WHERE pleg='fons/res' AND a IS NULL") == 1);
    CHECK(vault_resolve(v, "Fons/ACA/Relatiu.md", "../../Persones/Pere el Cerimoniós.md",
                        strlen("../../Persones/Pere el Cerimoniós.md")) > 0);
    vault_close(v);
}

static void test_nova(void)
{
    char root[600], rel[700], dest[700], err[256], *s, **l;
    size_t n;
    struct stat st;

    CHECK(eqfree(nova_nom("  Carta: «el rei» / f. 56v?  "), "Carta «el rei»  f. 56v"));
    CHECK(eqfree(nova_nom("[[x]]#^|*"), "x"));
    CHECK(eqfree(nova_nom("..."), ""));

    s = nova_text(NULL, "Fogatge", "pi-1", "2026-11-13 08:15", NULL);
    CHECK(strstr(s, "id: pi-1\n") && strstr(s, "created:") && strstr(s, "2026-11-13 08:15") && strstr(s, "# Fogatge\n"));
    CHECK(!strncmp(s, "---\n", 4));
    free(s);
    s = nova_text("---\ntipus: fons\n---\n# {{title}}\nCreada el {{date}} a les {{time}}; {{titol}}.\n",
                  "Reg. 12", "x7", "2026-11-13 08:15", "Pere III");
    CHECK(strstr(s, "tipus: fons\n") && strstr(s, "id: x7") && strstr(s, "anota: \"[[Pere III]]\"\n"));
    CHECK(strstr(s, "# Reg. 12\nCreada el 2026-11-13 a les 08:15; Reg. 12.\n"));
    {
        size_t line;

        CHECK(cm_front(s, strlen(s), &line, NULL, NULL) > 0);
    }
    free(s);

    snprintf(root, sizeof root, "%s/nova", tmpdir);
    mkdir(root, 0755);
    CHECK(nova_crea(root, "_pi", "Nota", "a\n", rel, sizeof rel, err, sizeof err) == 0 && !strcmp(rel, "_pi/Nota.md"));
    CHECK(nova_crea(root, "_pi", "Nota", "b\n", rel, sizeof rel, err, sizeof err) == 0 && !strcmp(rel, "_pi/Nota 2.md"));
    CHECK(nova_crea(root, "", "Arrel", "c\n", rel, sizeof rel, err, sizeof err) == 0 && !strcmp(rel, "Arrel.md"));
    CHECK(nova_crea(root, "x", "", "c\n", rel, sizeof rel, err, sizeof err) != 0);
    CHECK(nova_paperera(root, "_pi/Nota.md", dest, sizeof dest, err, sizeof err) == 0 && !strcmp(dest, ".trash/Nota.md"));
    CHECK(nova_crea(root, "_pi", "Nota", "d\n", rel, sizeof rel, err, sizeof err) == 0 && !strcmp(rel, "_pi/Nota.md"));
    CHECK(nova_paperera(root, "_pi/Nota.md", dest, sizeof dest, err, sizeof err) == 0 && !strcmp(dest, ".trash/Nota 2.md"));
    snprintf(rel, sizeof rel, "%s/.trash/Nota 2.md", root);
    CHECK(stat(rel, &st) == 0);

    /* plantilles: la carpeta de .obsidian/templates.json */
    put("nova/.obsidian/templates.json", "{\n  \"folder\": \"Models\"\n}\n");
    put("nova/Models/Fons.md", "---\ntipus: fons\n---\n");
    put("nova/Models/notes.txt", "no\n");
    nova_carpeta_plantilles(root, dest, sizeof dest);
    CHECK(!strcmp(dest, "Models"));
    n = nova_plantilles(root, &l);
    CHECK(n == 1 && !strcmp(l[0], "Models/Fons.md"));
    nova_llista_free(l, n);
}

int main(void)
{
    char cmd[600];

    setlocale(LC_ALL, "C.UTF-8");
    if (!mkdtemp(tmpdir)) {
        perror("mkdtemp");
        return 1;
    }
    test_fold();
    test_rc();
    test_front();
    test_scan();
    test_vault();
    test_relatiu();
    test_nova();
    snprintf(cmd, sizeof cmd, "rm -rf '%s'", tmpdir);
    if (system(cmd)) {
    }
    printf("Recerca: %d comprovacions, %d errors\n", checks, failures);
    return failures != 0;
}
