/* Proves de Fàbula (fabula/): make test-fabula */
#include "flat.h"
#include "project.h"
#include "yaml.h"

#include <errno.h>
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

static char tmpdir[] = "/tmp/fabula-test-XXXXXX";

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

static char *slurp(const char *rel)
{
    char path[512];
    static char buf[65536];
    FILE *f;
    size_t n;

    snprintf(path, sizeof path, "%s/%s", tmpdir, rel);
    f = fopen(path, "rb");
    if (!f)
        return NULL;
    n = fread(buf, 1, sizeof buf - 1, f);
    buf[n] = '\0';
    fclose(f);
    return buf;
}

static void test_yaml(void)
{
    char v[128];
    const char *t = "---\ntitol: El pou\nestat: \"revisat\"\n# comentari\n  niat: 1\nsinopsi: 'L''Anna'\nestat: duplicat\netiquetes: [a, \"b c\", 'd']\n---\n\nText: no clau\n";
    size_t n = strlen(t), rl;
    char *r, **it;

    CHECK(yaml_get(t, n, "titol", v, sizeof v) && !strcmp(v, "El pou"));
    CHECK(yaml_get(t, n, "estat", v, sizeof v) && !strcmp(v, "revisat"));   /* la primera */
    CHECK(yaml_get(t, n, "sinopsi", v, sizeof v) && !strcmp(v, "L'Anna"));
    CHECK(!yaml_get(t, n, "niat", v, sizeof v));       /* sagnada: no és plana */
    CHECK(!yaml_get(t, n, "Text", v, sizeof v));       /* fora de la capçalera */
    CHECK(!yaml_get(t, n, "titol2", v, sizeof v));
    CHECK(!yaml_get("Hola\n---\na: b\n---\n", 18, "a", v, sizeof v));   /* no és al principi */
    CHECK(!yaml_get("---\na: b\n", 9, "a", v, sizeof v));              /* sense tancar */
    CHECK(yaml_get("---\r\na: b\r\n---\r\n", 16, "a", v, sizeof v) && !strcmp(v, "b"));
    CHECK(yaml_get("---\na: b\n...\n", 13, "a", v, sizeof v) && !strcmp(v, "b"));
    CHECK(yaml_get("---\na:\n---\n", 11, "a", v, sizeof v) && v[0] == '\0');
    CHECK(yaml_get("---\nab: 1\na: 2\n---\n", 19, "a", v, sizeof v) && !strcmp(v, "2"));
    CHECK(yaml_get(t, n, "titol", v, 4) && !strcmp(v, "El "));          /* truncat */

    yaml_get(t, n, "etiquetes", v, sizeof v);
    CHECK(yaml_list(v, &it) == 3 && !strcmp(it[0], "a") && !strcmp(it[1], "b c") &&
          !strcmp(it[2], "d"));
    yaml_list_free(it, 3);
    CHECK(yaml_list("[]", &it) == 0 && it == NULL);
    CHECK(yaml_list("a, b", &it) == 0 && it == NULL);
    CHECK(yaml_list("[a,,b, ]", &it) == 2);
    yaml_list_free(it, 2);

    /* canviar només una línia */
    r = yaml_set(t, n, "estat", "final", 0, &rl);
    CHECK(rl == strlen(r));
    CHECK(!strncmp(r, "---\ntitol: El pou\nestat: final\n# comentari\n  niat: 1\n", 51));
    CHECK(strstr(r, "estat: duplicat\n") != NULL);    /* la segona no es toca */
    CHECK(strstr(r, "\n\nText: no clau\n") != NULL);
    free(r);
    /* afegir */
    r = yaml_set(t, n, "pov", "Anna", 0, &rl);
    CHECK(strstr(r, "etiquetes: [a, \"b c\", 'd']\npov: Anna\n---\n") != NULL);
    CHECK(rl == strlen(r));
    free(r);
    /* crear la capçalera */
    r = yaml_set("Hola\n", 5, "estat", "revisat", 0, &rl);
    CHECK(!strcmp(r, "---\nestat: revisat\n---\n\nHola\n") && rl == strlen(r));
    free(r);
    r = yaml_set("", 0, "estat", "x", 0, &rl);
    CHECK(!strcmp(r, "---\nestat: x\n---\n"));
    free(r);
    /* cometes */
    r = yaml_set("---\n---\n", 8, "sinopsi", "L'Anna: arriba #1", 0, &rl);
    CHECK(strstr(r, "sinopsi: \"L'Anna: arriba #1\"\n") != NULL);
    yaml_get(r, rl, "sinopsi", v, sizeof v);
    CHECK(!strcmp(v, "L'Anna: arriba #1"));
    free(r);
    r = yaml_set("---\n---\n", 8, "t", "a \"b\" \\ c", 0, &rl);
    yaml_get(r, rl, "t", v, sizeof v);
    CHECK(!strcmp(v, "a \"b\" \\ c"));
    free(r);
    r = yaml_set("---\n---\n", 8, "t", "[x, y]", 1, &rl);
    CHECK(strstr(r, "t: [x, y]\n") != NULL);
    free(r);
    /* CRLF es manté */
    r = yaml_set("---\r\na: b\r\n---\r\ntext\r\n", 22, "a", "c", 0, &rl);
    CHECK(!strcmp(r, "---\r\na: c\r\n---\r\ntext\r\n"));
    free(r);
    r = yaml_set("---\r\na: b\r\n---\r\n", 17, "z", "1", 0, &rl);
    CHECK(!strcmp(r, "---\r\na: b\r\nz: 1\r\n---\r\n"));
    free(r);
    /* idempotent */
    r = yaml_set(t, n, "estat", "revisat", 0, &rl);
    yaml_get(r, rl, "estat", v, sizeof v);
    CHECK(!strcmp(v, "revisat"));
    {
        size_t rl2;
        char *r2 = yaml_set(r, rl, "estat", "revisat", 0, &rl2);

        CHECK(rl2 == rl && !memcmp(r, r2, rl));   /* canviar-ho dos cops no fa res més */
        free(r2);
    }
    free(r);
}

static void test_noms(void)
{
    char *s;

    CHECK(fk_name_cmp("2-a.md", "10-a.md") < 0);
    CHECK(fk_name_cmp("10-a.md", "2-a.md") > 0);
    CHECK(fk_name_cmp("01-a.md", "02-a.md") < 0);
    CHECK(fk_name_cmp("a.md", "a.md") == 0);
    CHECK(fk_name_cmp("A.md", "b.md") < 0);
    CHECK(fk_name_cmp("cap", "capitol") < 0);
    CHECK(fk_name_cmp("1-x", "01-x") == 0);
    CHECK(fk_name_cmp("99999999999999999999-a", "100000000000000000000-a") < 0);

    s = fk_slug("Anna Puig");              CHECK(!strcmp(s, "anna-puig"));             free(s);
    s = fk_slug("L'arribada al Mas Roig!");CHECK(!strcmp(s, "l-arribada-al-mas-roig"));free(s);
    s = fk_slug("Ça va, això?");           CHECK(!strcmp(s, "ca-va-aixo"));            free(s);
    s = fk_slug("col·legi");               CHECK(!strcmp(s, "collegi"));               free(s);
    s = fk_slug("El Pare Nöel — I");       CHECK(!strcmp(s, "el-pare-noel-i"));        free(s);
    s = fk_slug("  ../../etc/passwd ");    CHECK(!strcmp(s, "etc-passwd"));            free(s);
    s = fk_slug("");                       CHECK(!strcmp(s, "sense-titol"));           free(s);
    s = fk_slug("---");                    CHECK(!strcmp(s, "sense-titol"));           free(s);
    s = fk_slug("\xFF\xFEok");             CHECK(!strcmp(s, "ok"));                    free(s);
    s = fk_slug("Año 1936");               CHECK(!strcmp(s, "ano-1936"));              free(s);
}

static void test_projecte(void)
{
    Project p;
    char err[256];
    struct stat st;
    char *t;
    time_t now;
    struct tm tm = { .tm_year = 126, .tm_mon = 10, .tm_mday = 11, .tm_hour = 10, .tm_isdst = -1 };

    now = mktime(&tm);

    put("projecte.md", "---\ntitol: La casa del pou\nautor: A\nobjectiu: 1000\ndata-limit: 2026-11-20\nestats: [esborrany, revisat, final]\n---\n\nTauler.\n");
    put("manuscrit/10-final.md", "# Final\n\nUna dues tres.\n");
    put("manuscrit/02-el-pou.md", "---\nestat: revisat\nsinopsi: L'Anna troba el pou\n---\n\n## El pou\n\nUn dos tres quatre.\n\n### Escena 1\n\ncinc sis.\n\n### Escena 2\n\nset vuit nou.\n\n#### Detall\n\ndeu.\n");
    put("manuscrit/01-comencament.md", "Sense capçalera ni yaml, quatre paraules.\n");
    put("manuscrit/notes.txt", "no compta\n");
    put("manuscrit/.ocult.md", "no compta\n");
    put("personatges/anna-puig.md", "---\ntipus: personatge\nalies: [Anneta]\n---\n\n# Anna Puig\n\nProtagonista alta.\n");
    put("llocs/mas-roig.md", "# Mas Roig\n\nCasa.\n");
    put("notes/idees.md", "idees soltes\n");
    put("zeta/x.md", "# X\n");
    put("plantilles/personatge.md", "plantilla\n");
    put(".paperera/vell.md", "res\n");
    {
        char a[512];

        snprintf(a, sizeof a, "%s/manuscrit/enllac.md", tmpdir);
        CHECK(symlink("02-el-pou.md", a) == 0);
        snprintf(a, sizeof a, "%s/llocs/fora", tmpdir);
        CHECK(symlink("/tmp", a) == 0);
    }

    CHECK(project_open(&p, tmpdir, err, sizeof err) == 0);
    CHECK(!strcmp(p.titol, "La casa del pou") && !strcmp(p.autor, "A") && !strcmp(p.idioma, "ca"));
    CHECK(p.objectiu == 1000 && p.nestats == 3 && !strcmp(p.estats[1], "revisat"));
    CHECK(p.manuscrit && !strcmp(p.manuscrit->title, "Manuscrit") && p.manuscrit->open);
    CHECK(!strcmp(p.top->kids[0]->name, "manuscrit"));
    CHECK(!strcmp(p.top->kids[1]->name, "personatges"));
    CHECK(!strcmp(p.top->kids[2]->name, "llocs"));
    CHECK(!strcmp(p.top->kids[3]->name, "notes"));
    CHECK(!strcmp(p.top->kids[4]->name, "zeta"));
    CHECK(p.top->nkids == 5);     /* .paperera i plantilles no són grups */

    /* capítols: ordre numèric, enllaços i ocults fora */
    CHECK(p.manuscrit->nkids == 3);
    {
        FNode *c1 = p.manuscrit->kids[0], *c2 = p.manuscrit->kids[1], *c3 = p.manuscrit->kids[2];

        CHECK(!strcmp(c1->name, "01-comencament.md") && !strcmp(c2->name, "02-el-pou.md") &&
              !strcmp(c3->name, "10-final.md"));
        CHECK(c1->kind == FK_CHAPTER && c1->nkids == 0);
        CHECK(!strcmp(c1->title, "Comencament"));      /* sense capçalera: del nom */
        CHECK(!strcmp(c3->title, "Final"));
        CHECK(!strcmp(c2->title, "El pou"));
        CHECK(c1->words == 6 && c3->words == 3);
        /* 02: Un dos tres quatre (4) + cinc sis (2) + set vuit nou (3) + deu (1) = 10; ni capçaleres ni YAML */
        CHECK(c2->words == 10);
        CHECK(c2->nkids == 2 && c2->kids[0]->kind == FK_SCENE);
        CHECK(!strcmp(c2->kids[0]->title, "Escena 1") && c2->kids[0]->words == 2);
        CHECK(!strcmp(c2->kids[1]->title, "Escena 2") && c2->kids[1]->words == 4);   /* amb #### Detall */
        CHECK(c2->kids[0]->line < c2->kids[1]->line && c2->kids[0]->line > 0);
        CHECK(!strcmp(c2->estat, "revisat") && c2->estat_set);
        CHECK(!strcmp(c1->estat, "esborrany") && !c1->estat_set);     /* per defecte */
        CHECK(!strcmp(c2->sinopsi, "L'Anna troba el pou") && c1->sinopsi[0] == '\0');
        CHECK(project_words(&p) == 19 && p.manuscrit->words == 19);
    }
    /* fitxes */
    {
        FNode *g = p.top->kids[1];

        CHECK(g->nkids == 1 && g->kids[0]->kind == FK_CARD);
        CHECK(!strcmp(g->kids[0]->title, "Anna Puig") && g->kids[0]->words == 2);   /* no compta l'YAML ni el títol */
        CHECK(g->words == 2);
        CHECK(!strcmp(p.top->kids[2]->title, "Llocs") && p.top->kids[2]->nkids == 1);
    }
    CHECK(p.top->kids[0]->kids[1]->parent == p.manuscrit);

    /* objectius: 19 de 1000, queden 981 en 10 dies (del 11 al 20, inclosos) */
    CHECK(project_days_left(&p, now) == 10);
    CHECK(project_daily_needed(&p, now) == 99);
    {
        struct tm late = tm;

        late.tm_mday = 21;
        CHECK(project_days_left(&p, mktime(&late)) == -1);
        CHECK(project_daily_needed(&p, mktime(&late)) == -1);
        late.tm_mday = 20;
        CHECK(project_days_left(&p, mktime(&late)) == 1);
    }

    /* estats */
    CHECK(!strcmp(project_next_estat(&p, "esborrany"), "revisat"));
    CHECK(!strcmp(project_next_estat(&p, "final"), "esborrany"));
    CHECK(!strcmp(project_next_estat(&p, "desconegut"), "esborrany"));

    /* canviar l'estat reescriu només la seva línia */
    {
        FNode *c2 = p.manuscrit->kids[1], *c1 = p.manuscrit->kids[0];

        chmod(c2->path, 0640);
        CHECK(project_set_estat(&p, c2, "final") == 0);
        CHECK(!strcmp(c2->estat, "final"));
        t = slurp("manuscrit/02-el-pou.md");
        CHECK(!strncmp(t, "---\nestat: final\nsinopsi: L'Anna troba el pou\n---\n\n## El pou\n", 61));
        CHECK(stat(c2->path, &st) == 0 && (st.st_mode & 07777) == 0640);
        CHECK(project_set_estat(&p, c1, "revisat") == 0 && c1->estat_set);
        t = slurp("manuscrit/01-comencament.md");
        CHECK(!strncmp(t, "---\nestat: revisat\n---\n\nSense capçalera", 38));
        CHECK(project_set_estat(&p, p.top, "x") == -1);                    /* arrel */
        CHECK(project_set_estat(&p, c2->kids[0], "x") == -1);              /* escena */
    }
    project_free(&p);

    /* es torna a obrir i els canvis hi són; les paraules no varien per l'YAML nou */
    CHECK(project_open(&p, tmpdir, err, sizeof err) == 0);
    CHECK(!strcmp(p.manuscrit->kids[1]->estat, "final") && !strcmp(p.manuscrit->kids[0]->estat, "revisat"));
    CHECK(p.manuscrit->kids[0]->words == 6 && project_words(&p) == 19);
    project_free(&p);

    /* errors */
    CHECK(project_open(&p, "/tmp/fabula-no-existeix-xyz", err, sizeof err) == -1 && strstr(err, "no és una carpeta"));
    {
        char sub[512];

        snprintf(sub, sizeof sub, "%s/notes", tmpdir);
        CHECK(project_open(&p, sub, err, sizeof err) == -1 && strstr(err, "projecte.md"));
    }
    /* projecte mínim: estats per defecte, sense objectiu */
    put("min/projecte.md", "res\n");
    put("min/manuscrit/01-a.md", "una\n");
    {
        char sub[512];

        snprintf(sub, sizeof sub, "%s/min", tmpdir);
        CHECK(project_open(&p, sub, err, sizeof err) == 0);
        CHECK(!strcmp(p.titol, "Min") && p.objectiu == 0 && p.nestats == 3);
        CHECK(project_daily_needed(&p, now) == -1 && project_days_left(&p, now) == -1);
        CHECK(project_words(&p) == 1);
        project_free(&p);
    }
}

static void test_flat(void)
{
    Project p;
    Flat f;
    char err[256], n[32];

    CHECK(project_open(&p, tmpdir, err, sizeof err) == 0);

    /* per defecte: els grups, i el manuscrit desplegat (només capítols) */
    flat_build(&f, &p, NULL);
    CHECK(f.n == 9 && f.rows[0].node == p.manuscrit && f.rows[0].depth == 0);
    CHECK(f.rows[1].depth == 1 && f.rows[1].node->kind == FK_CHAPTER);
    CHECK(flat_find(&f, p.manuscrit->kids[2]) == 3 && flat_find(&f, p.top) == -1);
    flat_free(&f);

    /* capítol desplegat: hi surten les escenes */
    p.manuscrit->kids[1]->open = 1;
    flat_build(&f, &p, "");
    CHECK(f.n == 11);
    CHECK(f.rows[3].node->kind == FK_SCENE && f.rows[3].depth == 2);
    flat_free(&f);

    /* filtre: sense majúscules ni accents, per títol i sinopsi; desplega els pares */
    flat_build(&f, &p, "POU");          /* el capítol 02, per la sinopsi i el títol */
    CHECK(f.n == 2 && f.rows[0].node == p.manuscrit && f.rows[1].node->kind == FK_CHAPTER);
    flat_free(&f);
    flat_build(&f, &p, "escena 2");     /* una escena: surten el capítol i el grup, desplegats */
    CHECK(f.n == 3 && f.rows[2].node->kind == FK_SCENE && f.rows[2].depth == 2);
    flat_free(&f);
    flat_build(&f, &p, "inexistent-xyz");
    CHECK(f.n == 0);
    flat_free(&f);
    flat_build(&f, &p, "anna");        /* sinopsi d'un capítol i títol d'una fitxa */
    CHECK(f.n == 4);
    flat_free(&f);
    project_free(&p);

    CHECK(fk_match("Cafè amb llet", "CAFE") && fk_match("Çà", "ca") && !fk_match("abc", "abd"));
    CHECK(fk_match("abc", "") && fk_match("L'Anna", "anna"));
    fk_fmt_num(0, n, sizeof n);        CHECK(!strcmp(n, "0"));
    fk_fmt_num(999, n, sizeof n);      CHECK(!strcmp(n, "999"));
    fk_fmt_num(1000, n, sizeof n);     CHECK(!strcmp(n, "1.000"));
    fk_fmt_num(42310, n, sizeof n);    CHECK(!strcmp(n, "42.310"));
    fk_fmt_num(1234567, n, sizeof n);  CHECK(!strcmp(n, "1.234.567"));
}

static void test_refresh(void)
{
    Project p;
    char err[256];
    FNode *c2;
    size_t before;

    CHECK(project_open(&p, tmpdir, err, sizeof err) == 0);
    before = project_words(&p);
    p.manuscrit->open = 0;
    c2 = p.manuscrit->kids[1];
    c2->open = 1;
    p.top->kids[1]->open = 1;

    /* sense canvis: res no varia, i es conserva el que és desplegat */
    CHECK(project_refresh(&p, err, sizeof err) == 0);
    CHECK(project_words(&p) == before && !p.manuscrit->open && p.manuscrit->kids[1]->open);
    CHECK(p.top->kids[1]->open && p.manuscrit->kids[1]->nkids == 2);
    CHECK(!strcmp(p.manuscrit->kids[1]->estat, "final"));

    /* un capítol canviat (mateixa mida, mateix segon: ho detecta pels ns) */
    put("manuscrit/01-comencament.md", "Sense capçalera ni yaml, quatre XXXXXXXXX.\n");
    CHECK(project_refresh(&p, err, sizeof err) == 0);
    CHECK(p.manuscrit->kids[0]->words == 6 && project_words(&p) == before);
    /* desat atòmic com el de Phileas (rename): mateixa mida, un altre inode */
    {
        char a[512], b[512];

        snprintf(a, sizeof a, "%s/manuscrit/01-comencament.md", tmpdir);
        snprintf(b, sizeof b, "%s/manuscrit/.tmp", tmpdir);
        put("manuscrit/.tmp", "Sense capçalera ni yaml, quatre YYYYYYYYY altres.\n");
        CHECK(rename(b, a) == 0);
        CHECK(project_refresh(&p, err, sizeof err) == 0);
        CHECK(p.manuscrit->kids[0]->words == 7);
    }
    put("manuscrit/01-comencament.md", "Un dos tres.\n\n### Nova escena\n\nquatre cinc.\n");
    CHECK(project_refresh(&p, err, sizeof err) == 0);
    CHECK(p.manuscrit->kids[0]->words == 5 && p.manuscrit->kids[0]->nkids == 1);
    CHECK(project_words(&p) == before - 6 + 5);

    /* fitxers nous i esborrats */
    put("manuscrit/03-nou.md", "# Nou\n\nuna dues.\n");
    {
        char a[512];

        snprintf(a, sizeof a, "%s/manuscrit/10-final.md", tmpdir);
        CHECK(unlink(a) == 0);
    }
    CHECK(project_refresh(&p, err, sizeof err) == 0);
    CHECK(p.manuscrit->nkids == 3);
    CHECK(!strcmp(p.manuscrit->kids[2]->name, "03-nou.md") && p.manuscrit->kids[2]->words == 2);

    /* canviar l'estat no obliga a rellegir-lo i es conserva en refrescar */
    CHECK(project_set_estat(&p, p.manuscrit->kids[2], "revisat") == 0);
    CHECK(project_refresh(&p, err, sizeof err) == 0);
    CHECK(!strcmp(p.manuscrit->kids[2]->estat, "revisat") && p.manuscrit->kids[2]->estat_set);
    project_free(&p);
}

static int has(const char *rel)
{
    char path[512];

    snprintf(path, sizeof path, "%s/%s", tmpdir, rel);
    return access(path, F_OK) == 0;
}

static FNode *chapter_named(Project *p, const char *name)
{
    if (!p->manuscrit)
        return NULL;
    for (size_t i = 0; i < p->manuscrit->nkids; i++)
        if (!strcmp(p->manuscrit->kids[i]->name, name))
            return p->manuscrit->kids[i];
    return NULL;
}

static void test_ops(void)
{
    Project p;
    char err[300], out[600], dir[512];
    FNode *g, *n;
    char *t;

    snprintf(dir, sizeof dir, "%s/ops", tmpdir);
    put("ops/projecte.md", "---\ntitol: Ops\n---\n");
    put("ops/manuscrit/01-a.md", "---\nestat: final\n---\n\n## A\n\nuna.\n");
    put("ops/manuscrit/02-b.md", "## B\n\ndues paraules.\n");
    put("ops/manuscrit/10-c.md", "## C\n\ntres paraules aqui.\n");
    put("ops/personatges/vell.md", "# Vell\n\nx\n");
    CHECK(project_open(&p, dir, err, sizeof err) == 0);

    /* capítol nou: número següent, plantilla de fàbrica, sense sobreescriure */
    CHECK(project_new_item(&p, NULL, "  Un títol: especial\n", out, sizeof out, err, sizeof err) == 0);
    CHECK(strstr(out, "/ops/manuscrit/20-un-titol-especial.md") != NULL);
    t = slurp("ops/manuscrit/20-un-titol-especial.md");
    CHECK(strstr(t, "estat: esborrany") && strstr(t, "## Un títol: especial\n") && strstr(t, "### Escena 1"));
    CHECK(project_refresh(&p, err, sizeof err) == 0);
    n = chapter_named(&p, "20-un-titol-especial.md");
    CHECK(n && p.manuscrit->nkids == 4 && !strcmp(n->title, "Un títol: especial") && n->nkids == 1);
    CHECK(n->estat_set && !strcmp(n->estat, "esborrany") && n->words == 0);
    CHECK(project_new_item(&p, p.manuscrit, "Un títol: especial", out, sizeof out, err, sizeof err) == 0);
    CHECK(strstr(out, "/30-un-titol-especial.md") != NULL);

    /* errors */
    CHECK(project_new_item(&p, NULL, "   ", out, sizeof out, err, sizeof err) == -1 && strstr(err, "buit"));

    /* fitxa: nom amb sufix si ja hi és; plantilla pròpia del projecte */
    g = p.top->kids[1];
    CHECK(!strcmp(g->name, "personatges"));
    CHECK(project_new_item(&p, g, "Anna Puig", out, sizeof out, err, sizeof err) == 0);
    CHECK(strstr(out, "/personatges/anna-puig.md") != NULL);
    CHECK(project_new_item(&p, g, "Anna Puig", out, sizeof out, err, sizeof err) == 0);
    CHECK(strstr(out, "/personatges/anna-puig-2.md") != NULL);
    t = slurp("ops/personatges/anna-puig.md");
    CHECK(strstr(t, "tipus: personatge") && strstr(t, "# Anna Puig") && strstr(t, "## Arc"));
    put("ops/plantilles/personatge.md", "# {{titol}} ({{data}})\n\nFitxa pròpia.\n");
    CHECK(project_new_item(&p, g, "Marta", out, sizeof out, err, sizeof err) == 0);
    t = slurp("ops/personatges/marta.md");
    CHECK(!strncmp(t, "# Marta (20", 11) && strstr(t, "Fitxa pròpia.") && !strstr(t, "{{"));

    /* reanomenar: títol a la capçalera, el text no es toca; el nom segueix el títol */
    CHECK(project_refresh(&p, err, sizeof err) == 0);
    g = p.top->kids[1];
    n = g->kids[0];
    CHECK(!strcmp(n->name, "anna-puig-2.md") || !strcmp(n->name, "anna-puig.md"));
    n = NULL;
    for (size_t i = 0; i < g->nkids; i++)
        if (!strcmp(g->kids[i]->name, "anna-puig.md"))
            n = g->kids[i];
    CHECK(n != NULL);
    CHECK(project_rename(&p, n, "Anna Puig i Roca", out, sizeof out, err, sizeof err) == 0);
    CHECK(strstr(out, "/personatges/anna-puig-i-roca.md") && !has("ops/personatges/anna-puig.md"));
    t = slurp("ops/personatges/anna-puig-i-roca.md");
    CHECK(strstr(t, "titol: Anna Puig i Roca\n") && strstr(t, "# Anna Puig\n") && strstr(t, "## Arc"));
    CHECK(project_refresh(&p, err, sizeof err) == 0);
    {
        int found = 0;

        for (size_t i = 0; i < p.top->kids[1]->nkids; i++)
            found |= !strcmp(p.top->kids[1]->kids[i]->title, "Anna Puig i Roca");
        CHECK(found);
    }
    /* capítol: es conserva el número */
    n = chapter_named(&p, "20-un-titol-especial.md");
    CHECK(project_rename(&p, n, "Nou nom", out, sizeof out, err, sizeof err) == 0);
    CHECK(has("ops/manuscrit/20-nou-nom.md") && !has("ops/manuscrit/20-un-titol-especial.md"));
    /* nom ocupat: no sobreescriu */
    CHECK(project_refresh(&p, err, sizeof err) == 0);
    n = chapter_named(&p, "30-un-titol-especial.md");
    CHECK(project_rename(&p, n, "Nou nom", out, sizeof out, err, sizeof err) == 0);   /* 30-nou-nom: lliure */
    n = chapter_named(&p, "02-b.md");
    CHECK(n != NULL);
    CHECK(project_rename(&p, n, "a", out, sizeof out, err, sizeof err) == 0 && has("ops/manuscrit/02-a.md"));
    CHECK(project_refresh(&p, err, sizeof err) == 0);
    n = chapter_named(&p, "01-a.md");
    CHECK(project_rename(&p, n, "a", out, sizeof out, err, sizeof err) == 0);          /* ja és 01-a: res a canviar */
    n = chapter_named(&p, "10-c.md");
    CHECK(project_rename(&p, n, "   ", out, sizeof out, err, sizeof err) == -1);
    CHECK(project_rename(&p, p.manuscrit, "x", out, sizeof out, err, sizeof err) == -1);
    CHECK(project_rename(&p, p.manuscrit->kids[0]->nkids ? p.manuscrit->kids[0]->kids[0] : p.manuscrit, "x", out, sizeof out, err, sizeof err) == -1);

    /* nom ocupat per un altre fitxer: no el trepitja (el títol sí que ha canviat) */
    n = NULL;
    for (size_t i = 0; i < p.top->kids[1]->nkids; i++)
        if (!strcmp(p.top->kids[1]->kids[i]->name, "vell.md"))
            n = p.top->kids[1]->kids[i];
    CHECK(n != NULL);
    CHECK(project_rename(&p, n, "Marta", out, sizeof out, err, sizeof err) == -1 && strstr(err, "marta.md"));
    CHECK(has("ops/personatges/vell.md") && strstr(slurp("ops/personatges/marta.md"), "Fitxa pròpia."));

    /* paperera: es mou, no s'esborra; dos amb el mateix nom no es trepitgen */
    CHECK(project_refresh(&p, err, sizeof err) == 0);
    n = chapter_named(&p, "10-c.md");
    CHECK(project_trash(&p, n, err, sizeof err) == 0);
    CHECK(!has("ops/manuscrit/10-c.md"));
    {
        char cmd[700];

        snprintf(cmd, sizeof cmd, "ls '%s/ops/.paperera' | grep -c -- '-10-c.md$' >/dev/null", tmpdir);
        CHECK(system(cmd) == 0);
    }
    put("ops/manuscrit/10-c.md", "## C2\n\nnou.\n");
    CHECK(project_refresh(&p, err, sizeof err) == 0);
    CHECK(project_trash(&p, chapter_named(&p, "10-c.md"), err, sizeof err) == 0);
    {
        char cmd[700];

        snprintf(cmd, sizeof cmd, "test \"$(ls '%s/ops/.paperera' | wc -l)\" = 2", tmpdir);
        CHECK(system(cmd) == 0);
    }
    CHECK(project_trash(&p, p.manuscrit, err, sizeof err) == -1);
    project_free(&p);
}

static void test_move(void)
{
    Project p;
    char err[300], out[600], dir[512];

    snprintf(dir, sizeof dir, "%s/mv", tmpdir);
    put("mv/projecte.md", "x\n");
    put("mv/manuscrit/01-a.md", "a\n");
    put("mv/manuscrit/02-b.md", "b\n");
    put("mv/manuscrit/10-c.md", "c\n");
    CHECK(project_open(&p, dir, err, sizeof err) == 0);

    CHECK(project_move(&p, p.manuscrit->kids[0], -1, out, sizeof out, err, sizeof err) == -1 && strstr(err, "primer"));
    CHECK(project_move(&p, p.manuscrit->kids[2], 1, out, sizeof out, err, sizeof err) == -1 && strstr(err, "últim"));
    /* intercanvi de números amb el veí: 02-b i 10-c */
    CHECK(project_move(&p, p.manuscrit->kids[1], 1, out, sizeof out, err, sizeof err) == 0);
    CHECK(strstr(out, "/mv/manuscrit/10-b.md") != NULL);
    CHECK(has("mv/manuscrit/10-b.md") && has("mv/manuscrit/02-c.md") && !has("mv/manuscrit/02-b.md"));
    CHECK(slurp("mv/manuscrit/10-b.md")[0] == 'b');                       /* el contingut va amb el fitxer */
    CHECK(project_refresh(&p, err, sizeof err) == 0);
    CHECK(!strcmp(p.manuscrit->kids[1]->name, "02-c.md") && !strcmp(p.manuscrit->kids[2]->name, "10-b.md"));
    /* i enrere */
    CHECK(project_move(&p, p.manuscrit->kids[2], -1, out, sizeof out, err, sizeof err) == 0);
    CHECK(has("mv/manuscrit/02-b.md") && has("mv/manuscrit/10-c.md"));
    project_free(&p);

    /* números repetits o absents: es renumera tot en passos de 10 */
    put("mv2/projecte.md", "x\n");
    put("mv2/manuscrit/1-a.md", "a\n");
    put("mv2/manuscrit/01-b.md", "b\n");
    put("mv2/manuscrit/x.md", "x\n");
    snprintf(dir, sizeof dir, "%s/mv2", tmpdir);
    CHECK(project_open(&p, dir, err, sizeof err) == 0);
    CHECK(!strcmp(p.manuscrit->kids[0]->name, "1-a.md") && !strcmp(p.manuscrit->kids[2]->name, "x.md"));
    CHECK(project_move(&p, p.manuscrit->kids[1], -1, out, sizeof out, err, sizeof err) == 0);
    CHECK(strstr(out, "/mv2/manuscrit/10-b.md") != NULL);
    CHECK(has("mv2/manuscrit/10-b.md") && has("mv2/manuscrit/20-a.md") && has("mv2/manuscrit/30-x.md"));
    CHECK(!has("mv2/manuscrit/1-a.md") && !has("mv2/manuscrit/01-b.md") && !has("mv2/manuscrit/x.md"));
    CHECK(slurp("mv2/manuscrit/20-a.md")[0] == 'a' && slurp("mv2/manuscrit/30-x.md")[0] == 'x');
    CHECK(project_move(&p, p.manuscrit, 1, out, sizeof out, err, sizeof err) == -1);      /* no és un capítol */
    project_free(&p);

    /* sense carpeta de manuscrit: el primer capítol la crea */
    put("mv3/projecte.md", "x\n");
    snprintf(dir, sizeof dir, "%s/mv3", tmpdir);
    CHECK(project_open(&p, dir, err, sizeof err) == 0 && p.manuscrit == NULL);
    CHECK(project_new_item(&p, NULL, "Primer", out, sizeof out, err, sizeof err) == 0);
    CHECK(has("mv3/manuscrit/10-primer.md"));
    CHECK(project_refresh(&p, err, sizeof err) == 0 && p.manuscrit && p.manuscrit->nkids == 1);
    project_free(&p);
}

static void test_create(void)
{
    Project p;
    char err[300], out[600];

    CHECK(project_create(tmpdir, "La casa: del pou", out, sizeof out, err, sizeof err) == 0);
    CHECK(strstr(out, "/la-casa-del-pou") != NULL && has("la-casa-del-pou/projecte.md"));
    CHECK(has("la-casa-del-pou/manuscrit") && has("la-casa-del-pou/personatges") &&
          has("la-casa-del-pou/llocs") && has("la-casa-del-pou/recerca") && has("la-casa-del-pou/notes"));
    CHECK(project_open(&p, out, err, sizeof err) == 0);
    CHECK(!strcmp(p.titol, "La casa: del pou") && p.objectiu == 0 && p.nestats == 3);   /* cometes a l'YAML */
    CHECK(p.top->nkids == 5 && p.manuscrit && p.manuscrit->nkids == 0);
    /* es pot començar a escriure */
    CHECK(project_new_item(&p, NULL, "Capítol u", out, sizeof out, err, sizeof err) == 0);
    CHECK(project_refresh(&p, err, sizeof err) == 0 && p.manuscrit->nkids == 1);
    project_free(&p);
    /* no trepitja un projecte existent, però sí una carpeta buida */
    CHECK(project_create(tmpdir, "La casa: del pou", out, sizeof out, err, sizeof err) == -1 && strstr(err, "ja existeix"));
    CHECK(project_create(tmpdir, "  ", out, sizeof out, err, sizeof err) == -1);
    {
        char a[512];

        snprintf(a, sizeof a, "%s/buida", tmpdir);
        CHECK(mkdir(a, 0755) == 0);
    }
    CHECK(project_create(tmpdir, "Buida", out, sizeof out, err, sizeof err) == 0 && has("buida/projecte.md"));
}

static void rmtree(const char *dir)
{
    char cmd[600];

    snprintf(cmd, sizeof cmd, "rm -rf '%s'", dir);
    if (system(cmd)) {}
}

int main(void)
{
    setlocale(LC_ALL, "");
    if (!mkdtemp(tmpdir)) {
        perror("mkdtemp");
        return 2;
    }
    test_yaml();
    test_noms();
    test_projecte();
    test_flat();
    test_refresh();
    test_ops();
    test_move();
    test_create();
    rmtree(tmpdir);
    printf("%d comprovacions, %d errors\n", checks, failures);
    return failures != 0;
}
