#include "vault.h"

#include "fold.h"
#include "front.h"
#include "mdscan.h"
#include "util.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>

struct Vault {
    char    *root;
    sqlite3 *db;
};

static const char *ESQUEMA =
    "CREATE TABLE IF NOT EXISTS meta(clau TEXT PRIMARY KEY, valor TEXT);"
    "CREATE TABLE IF NOT EXISTS fitxes("
    "  id INTEGER PRIMARY KEY, cami TEXT UNIQUE NOT NULL, carpeta TEXT NOT NULL,"
    "  nom TEXT NOT NULL, nom_min TEXT NOT NULL, cami_min TEXT NOT NULL,"
    "  tipus TEXT, data TEXT, uid TEXT, paraules INT,"
    "  mtime INT, mida INT, ino INT);"
    "CREATE TABLE IF NOT EXISTS propietats(fitxa INT, clau TEXT, valor TEXT, ordre INT);"
    "CREATE TABLE IF NOT EXISTS etiquetes(fitxa INT, etiqueta TEXT, etiqueta_min TEXT, linia INT);"
    "CREATE TABLE IF NOT EXISTS alies(fitxa INT, alies TEXT, pleg TEXT);"
    "CREATE TABLE IF NOT EXISTS enllacos(de INT, linia INT, desti TEXT, pleg TEXT,"
    "  seccio TEXT, bloc INT, text TEXT, relacio TEXT, incrust INT, adjunt INT,"
    "  context TEXT, a INT);"
    "CREATE TABLE IF NOT EXISTS blocs(fitxa INT, bloc TEXT, linia INT);"
    "CREATE VIRTUAL TABLE IF NOT EXISTS text USING fts5(nom, props, cos, content='',"
    "  contentless_delete=1, detail=none, tokenize='unicode61 remove_diacritics 2');"
    "CREATE INDEX IF NOT EXISTS i_fitxes_nom ON fitxes(nom_min);"
    "CREATE INDEX IF NOT EXISTS i_fitxes_carpeta ON fitxes(carpeta);"
    "CREATE INDEX IF NOT EXISTS i_fitxes_data ON fitxes(data);"
    "CREATE INDEX IF NOT EXISTS i_prop ON propietats(fitxa);"
    "CREATE INDEX IF NOT EXISTS i_prop_clau ON propietats(clau, valor);"
    "CREATE INDEX IF NOT EXISTS i_etiq ON etiquetes(etiqueta_min, fitxa);"
    "CREATE INDEX IF NOT EXISTS i_etiq_f ON etiquetes(fitxa);"
    "CREATE INDEX IF NOT EXISTS i_alies ON alies(pleg);"
    "CREATE INDEX IF NOT EXISTS i_alies_f ON alies(fitxa);"
    "CREATE INDEX IF NOT EXISTS i_enll_a ON enllacos(a);"
    "CREATE INDEX IF NOT EXISTS i_enll_de ON enllacos(de);"
    "CREATE INDEX IF NOT EXISTS i_blocs ON blocs(fitxa);";

static const char *BUIDA =
    "DROP TABLE IF EXISTS fitxes; DROP TABLE IF EXISTS propietats;"
    "DROP TABLE IF EXISTS etiquetes; DROP TABLE IF EXISTS alies;"
    "DROP TABLE IF EXISTS enllacos; DROP TABLE IF EXISTS blocs;"
    "DROP TABLE IF EXISTS text; DROP TABLE IF EXISTS meta;";

/* Resolució dels enllaços, com Obsidian: sense distingir majúscules,
 * [[Nom]] pel nom de fitxer (si n'hi ha diversos, el camí més curt),
 * [[Carpeta/Nom]] pel camí (sencer o pel final), i si no, pels àlies.
 * [[#Secció]] (destí buit) apunta a la mateixa nota. */
static const char *RESOL =
    "UPDATE enllacos SET a = CASE WHEN pleg = '' THEN de WHEN adjunt THEN NULL ELSE COALESCE("
    "  CASE WHEN instr(pleg, '/') = 0"
    "    THEN (SELECT id FROM fitxes f WHERE f.nom_min = enllacos.pleg ORDER BY length(f.cami), f.cami LIMIT 1)"
    "    ELSE (SELECT id FROM fitxes f WHERE f.cami_min = enllacos.pleg"
    "          OR substr(f.cami_min, -length(enllacos.pleg) - 1) = '/' || enllacos.pleg"
    "          ORDER BY length(f.cami) LIMIT 1) END,"
    "  (SELECT fitxa FROM alies WHERE alies.pleg = enllacos.pleg LIMIT 1)) END";

/* Després d'uns pocs canvis només cal tornar a resoldre els enllaços que hi
 * poden haver canviat: els de les notes tocades, els que hi apuntaven, els
 * trencats, els que tenen com a destí un nom o àlies nou i els de camí. */
static const char *RESOL_PARCIAL =
    " WHERE de IN (SELECT id FROM temp._ids) OR a IN (SELECT id FROM temp._ids) OR a IS NULL"
    " OR pleg IN (SELECT pleg FROM temp._plegs) OR instr(pleg, '/') > 0";

static const char *TEMP =
    "CREATE TEMP TABLE IF NOT EXISTS _ids(id INTEGER PRIMARY KEY);"
    "CREATE TEMP TABLE IF NOT EXISTS _plegs(pleg TEXT PRIMARY KEY);"
    "DELETE FROM temp._ids; DELETE FROM temp._plegs;";

static void seterr(char *err, size_t errsz, const char *fmt, const char *a, const char *b)
{
    if (err && errsz)
        snprintf(err, errsz, fmt, a ? a : "", b ? b : "");
}

static int exec(Vault *v, const char *sql, char *err, size_t errsz)
{
    char *e = NULL;

    if (sqlite3_exec(v->db, sql, NULL, NULL, &e) != SQLITE_OK) {
        seterr(err, errsz, "índex: %s%s", e, NULL);
        sqlite3_free(e);
        return -1;
    }
    return 0;
}

sqlite3 *vault_db(const Vault *v) { return v->db; }
const char *vault_root(const Vault *v) { return v->root; }

void vault_fmt_num(char *out, size_t outsz, long long n)
{
    char tmp[32];
    int len, o = 0, neg = n < 0;

    len = snprintf(tmp, sizeof tmp, "%lld", neg ? -n : n);
    if (neg && o + 1 < (int)outsz)
        out[o++] = '-';
    for (int i = 0; i < len && o + 2 < (int)outsz; i++) {
        if (i && (len - i) % 3 == 0)
            out[o++] = '.';
        out[o++] = tmp[i];
    }
    out[o] = '\0';
}

/* ---------- obrir ---------- */

Vault *vault_open(const char *root, char *err, size_t errsz)
{
    char path[4096];
    struct stat st;
    Vault *v;
    sqlite3_stmt *s;
    int ok = 0;

    if (stat(root, &st) || !S_ISDIR(st.st_mode)) {
        seterr(err, errsz, "%s no és una carpeta%s", root, NULL);
        return NULL;
    }
    snprintf(path, sizeof path, "%s/%s", root, VAULT_DIR);
    if (mkdir(path, 0755) && errno != EEXIST) {
        seterr(err, errsz, "no es pot crear %s: %s", path, strerror(errno));
        return NULL;
    }
    v = xmalloc(sizeof *v);
    v->root = xstrdup(root);
    while (strlen(v->root) > 1 && v->root[strlen(v->root) - 1] == '/')
        v->root[strlen(v->root) - 1] = '\0';
    snprintf(path, sizeof path, "%s/%s", v->root, VAULT_DB);
    if (sqlite3_open(path, &v->db) != SQLITE_OK) {
        seterr(err, errsz, "no es pot obrir %s: %s", path, sqlite3_errmsg(v->db));
        vault_close(v);
        return NULL;
    }
    sqlite3_busy_timeout(v->db, 3000);
    if (exec(v, "PRAGMA journal_mode=WAL; PRAGMA synchronous=NORMAL; PRAGMA cache_size=-8000;"
                "PRAGMA temp_store=MEMORY;", err, errsz)) {
        vault_close(v);
        return NULL;
    }
    /* versió de l'esquema: si no és la nostra, es refà */
    if (sqlite3_prepare_v2(v->db, "SELECT valor FROM meta WHERE clau='esquema'", -1, &s, NULL) == SQLITE_OK) {
        if (sqlite3_step(s) == SQLITE_ROW && !strcmp((const char *)sqlite3_column_text(s, 0), VAULT_SCHEMA))
            ok = 1;
        sqlite3_finalize(s);
    }
    if (!ok && (exec(v, BUIDA, err, errsz) || exec(v, ESQUEMA, err, errsz) ||
                exec(v, "INSERT INTO meta VALUES('esquema', '" VAULT_SCHEMA "')", err, errsz))) {
        vault_close(v);
        return NULL;
    }
    return v;
}

void vault_close(Vault *v)
{
    if (!v)
        return;
    if (v->db)
        sqlite3_close(v->db);
    free(v->root);
    free(v);
}

/* ---------- recórrer el vault ---------- */

typedef struct {
    char     *rel;
    long long mtime, mida, ino;
    long long id;           /* a la base de dades (0 = nova) */
    int       vist;
} Entrada;

typedef struct {
    Entrada *e;
    size_t   n, cap;
} Llista;

static void afegeix(Llista *l, const char *rel, const struct stat *st)
{
    if (l->n == l->cap) {
        l->cap = l->cap ? 2 * l->cap : 256;
        l->e = xrealloc(l->e, l->cap * sizeof *l->e);
    }
    l->e[l->n].rel = xstrdup(rel);
    l->e[l->n].mtime = (long long)st->st_mtim.tv_sec * 1000000000LL + st->st_mtim.tv_nsec;
    l->e[l->n].mida = st->st_size;
    l->e[l->n].ino = st->st_ino;
    l->e[l->n].id = 0;
    l->e[l->n].vist = 0;
    l->n++;
}

static void allibera(Llista *l)
{
    for (size_t i = 0; i < l->n; i++)
        free(l->e[i].rel);
    free(l->e);
    l->e = NULL;
    l->n = l->cap = 0;
}

static int es_md(const char *nom)
{
    size_t n = strlen(nom);

    return n > 3 && !strcmp(nom + n - 3, ".md");
}

/* Les carpetes i fitxers que comencen per «.» (.obsidian, .trash, .recerca,
 * .git…) no són del vault. */
static void recorre(const char *root, const char *rel, Llista *out)
{
    char dir[4096];
    DIR *d;
    struct dirent *e;

    snprintf(dir, sizeof dir, "%s%s%s", root, *rel ? "/" : "", rel);
    d = opendir(dir);
    if (!d)
        return;
    while ((e = readdir(d))) {
        char r[4096], c[8192];
        struct stat st;

        if (e->d_name[0] == '.')
            continue;
        snprintf(r, sizeof r, "%s%s%s", rel, *rel ? "/" : "", e->d_name);
        snprintf(c, sizeof c, "%s/%s", root, r);
        if (lstat(c, &st))
            continue;
        if (S_ISDIR(st.st_mode))
            recorre(root, r, out);
        else if (S_ISREG(st.st_mode) && es_md(e->d_name))
            afegeix(out, r, &st);
    }
    closedir(d);
}

static int per_cami(const void *a, const void *b)
{
    return strcmp(((const Entrada *)a)->rel, ((const Entrada *)b)->rel);
}

/* ---------- indexar una nota ---------- */

typedef struct {
    sqlite3_stmt *fitxa_ins, *fitxa_upd, *prop, *etiq, *alies, *enll, *bloc, *text;
    sqlite3_stmt *del_prop, *del_etiq, *del_alies, *del_enll, *del_bloc, *del_text, *del_fitxa;
    sqlite3_stmt *tocat, *nom_nou;
} Sents;

static int prepara(Vault *v, Sents *s)
{
    if (sqlite3_exec(v->db, TEMP, NULL, NULL, NULL) != SQLITE_OK)
        return -1;
#define P(x, q) if (sqlite3_prepare_v2(v->db, q, -1, &s->x, NULL) != SQLITE_OK) return -1
    P(fitxa_ins, "INSERT INTO fitxes(cami,carpeta,nom,nom_min,cami_min,tipus,data,uid,paraules,mtime,mida,ino)"
                 " VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12)");
    P(fitxa_upd, "UPDATE fitxes SET carpeta=?2,nom=?3,nom_min=?4,cami_min=?5,tipus=?6,data=?7,uid=?8,"
                 "paraules=?9,mtime=?10,mida=?11,ino=?12 WHERE id=?13");
    P(prop, "INSERT INTO propietats(fitxa,clau,valor,ordre) VALUES(?,?,?,?)");
    P(etiq, "INSERT INTO etiquetes(fitxa,etiqueta,etiqueta_min,linia) VALUES(?,?,?,?)");
    P(alies, "INSERT INTO alies(fitxa,alies,pleg) VALUES(?,?,?)");
    P(enll, "INSERT INTO enllacos(de,linia,desti,pleg,seccio,bloc,text,relacio,incrust,adjunt,context)"
            " VALUES(?,?,?,?,?,?,?,?,?,?,?)");
    P(bloc, "INSERT INTO blocs(fitxa,bloc,linia) VALUES(?,?,?)");
    P(text, "INSERT INTO text(rowid,nom,props,cos) VALUES(?,?,?,?)");
    P(del_prop, "DELETE FROM propietats WHERE fitxa=?");
    P(del_etiq, "DELETE FROM etiquetes WHERE fitxa=?");
    P(del_alies, "DELETE FROM alies WHERE fitxa=?");
    P(del_enll, "DELETE FROM enllacos WHERE de=?");
    P(del_bloc, "DELETE FROM blocs WHERE fitxa=?");
    P(del_text, "DELETE FROM text WHERE rowid=?");
    P(del_fitxa, "DELETE FROM fitxes WHERE id=?");
    P(tocat, "INSERT OR IGNORE INTO temp._ids VALUES(?)");
    P(nom_nou, "INSERT OR IGNORE INTO temp._plegs VALUES(?)");
#undef P
    return 0;
}

static void finalitza(Sents *s)
{
    sqlite3_stmt **all = (sqlite3_stmt **)s;

    for (size_t i = 0; i < sizeof *s / sizeof *all; i++)
        sqlite3_finalize(all[i]);
}

static int pas(sqlite3_stmt *s)
{
    int r = sqlite3_step(s);

    sqlite3_reset(s);
    sqlite3_clear_bindings(s);
    return r == SQLITE_DONE || r == SQLITE_ROW ? 0 : -1;
}

static void treu_fills(Sents *s, long long id)
{
    sqlite3_stmt *d[] = { s->del_prop, s->del_etiq, s->del_alies, s->del_enll, s->del_bloc, s->del_text };

    for (size_t i = 0; i < sizeof d / sizeof *d; i++) {
        sqlite3_bind_int64(d[i], 1, id);
        pas(d[i]);
    }
}

static void toca(Sents *s, long long id)
{
    sqlite3_bind_int64(s->tocat, 1, id);
    pas(s->tocat);
}

static void nom_nou(Sents *s, const char *pleg)
{
    sqlite3_bind_text(s->nom_nou, 1, pleg, -1, SQLITE_TRANSIENT);
    pas(s->nom_nou);
}

/* Dades recollides d'una nota abans d'escriure-les */
typedef struct {
    const char *k, *v;
    size_t kl, vl;
    int idx;
} Prop;

typedef struct {
    Prop  *p;
    size_t n, cap;
} Props;

static void prop_cb(const char *k, size_t kl, const char *v, size_t vl, int idx, void *ud)
{
    Props *ps = ud;

    if (ps->n == ps->cap) {
        ps->cap = ps->cap ? 2 * ps->cap : 16;
        ps->p = xrealloc(ps->p, ps->cap * sizeof *ps->p);
    }
    ps->p[ps->n++] = (Prop){ k, v, kl, vl, idx };
}

static int clau(const Prop *p, const char *k)
{
    return p->kl == strlen(k) && !strncasecmp(p->k, k, p->kl);
}

typedef struct {
    Sents      *s;
    long long   id;
    const char *relacio;     /* clau YAML quan es llegeixen les propietats */
    size_t      relacio_len;
    int         errors;
} Ctx;

/* [[imatge.png]], [[document.pdf]]: un adjunt, no una nota (no és trencat) */
static int te_extensio_no_md(const char *t, size_t n)
{
    size_t i = n, ext;

    while (i > 0 && t[i - 1] != '.' && t[i - 1] != '/')
        i--;
    if (i == 0 || t[i - 1] != '.')
        return 0;
    ext = n - i;
    if (ext < 2 || ext > 4 || (ext == 2 && !strncasecmp(t + i, "md", 2)))
        return 0;
    for (size_t k = i; k < n; k++)
        if (!((t[k] >= 'a' && t[k] <= 'z') || (t[k] >= 'A' && t[k] <= 'Z') || (t[k] >= '0' && t[k] <= '9')))
            return 0;
    return 1;
}

/* Context: la línia, retallada a ~200 bytes sense partir un caràcter */
static size_t context_len(const char *s, size_t n)
{
    if (n <= 200)
        return n;
    n = 200;
    while (n && ((unsigned char)s[n] & 0xC0) == 0x80)
        n--;
    return n;
}

static void tag(Ctx *c, const char *t, size_t n, size_t linia)
{
    char *min;

    while (n && *t == '#') {
        t++;
        n--;
    }
    if (!n)
        return;
    min = cm_lower(t, n);
    sqlite3_bind_int64(c->s->etiq, 1, c->id);
    sqlite3_bind_text(c->s->etiq, 2, t, (int)n, SQLITE_TRANSIENT);
    sqlite3_bind_text(c->s->etiq, 3, min, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(c->s->etiq, 4, (long long)linia);
    c->errors += pas(c->s->etiq) != 0;
    free(min);
}

static void ref_cb(const CmRef *r, void *ud)
{
    Ctx *c = ud;
    sqlite3_stmt *e = c->s->enll;

    switch (r->kind) {
    case CM_TAG:
        tag(c, r->target, r->target_len, r->line);
        break;
    case CM_BLOCK:
        sqlite3_bind_int64(c->s->bloc, 1, c->id);
        sqlite3_bind_text(c->s->bloc, 2, r->target, (int)r->target_len, SQLITE_TRANSIENT);
        sqlite3_bind_int64(c->s->bloc, 3, (long long)r->line);
        c->errors += pas(c->s->bloc) != 0;
        break;
    case CM_LINK: {
        char *pleg = cm_lower(r->target, r->target_len);

        sqlite3_bind_int64(e, 1, c->id);
        sqlite3_bind_int64(e, 2, (long long)r->line);
        sqlite3_bind_text(e, 3, r->target, (int)r->target_len, SQLITE_TRANSIENT);
        sqlite3_bind_text(e, 4, pleg, -1, SQLITE_TRANSIENT);
        if (r->section)
            sqlite3_bind_text(e, 5, r->section, (int)r->section_len, SQLITE_TRANSIENT);
        sqlite3_bind_int(e, 6, r->block);
        if (r->alias)
            sqlite3_bind_text(e, 7, r->alias, (int)r->alias_len, SQLITE_TRANSIENT);
        if (c->relacio)
            sqlite3_bind_text(e, 8, c->relacio, (int)c->relacio_len, SQLITE_TRANSIENT);
        sqlite3_bind_int(e, 9, r->embed);
        sqlite3_bind_int(e, 10, te_extensio_no_md(r->target, r->target_len));
        sqlite3_bind_text(e, 11, r->ctx, (int)context_len(r->ctx, r->ctx_len), SQLITE_TRANSIENT);
        c->errors += pas(e) != 0;
        free(pleg);
        break;
    }
    }
}

static size_t paraules(const char *s, size_t n)
{
    size_t w = 0;
    int dins = 0;

    for (size_t i = 0; i < n; i++) {
        unsigned char b = (unsigned char)s[i];
        int sep = b == ' ' || b == '\n' || b == '\t' || b == '\r';

        if (!sep && !dins)
            w++;
        dins = !sep;
    }
    return w;
}

/* Llegeix i indexa una nota. id = 0: nova; si no, se substitueix. */
static int indexa(Vault *v, Sents *s, const Entrada *en)
{
    char path[8192];
    FILE *f;
    char *buf, *nom, *nom_min, *cami_min, *cos_fts, *props_txt;
    const char *carpeta_fi, *tipus = NULL, *data = NULL, *uid = NULL;
    size_t tipus_l = 0, data_l = 0, uid_l = 0;
    size_t n, cos_off, body_line, rl = strlen(en->rel), pl = 0;
    long long id = en->id;
    Props ps = { NULL, 0, 0 };
    Ctx c;
    int r = 0;

    snprintf(path, sizeof path, "%s/%s", v->root, en->rel);
    f = fopen(path, "rb");
    if (!f)
        return 0;                   /* ha desaparegut entremig: la propera vegada */
    buf = xmalloc((size_t)en->mida + 1);
    n = fread(buf, 1, (size_t)en->mida, f);
    fclose(f);
    buf[n] = '\0';

    cos_off = cm_front(buf, n, &body_line, prop_cb, &ps);
    for (size_t i = 0; i < ps.n; i++) {
        const Prop *p = &ps.p[i];

        if (clau(p, "tipus") && !tipus) {
            tipus = p->v;
            tipus_l = p->vl;
        } else if (clau(p, "data") && !data) {
            data = p->v;
            data_l = p->vl;
        } else if (clau(p, "id") && !uid) {
            uid = p->v;
            uid_l = p->vl;
        }
        pl += p->vl + 1;
    }

    carpeta_fi = strrchr(en->rel, '/');
    nom = xstrdup(carpeta_fi ? carpeta_fi + 1 : en->rel);
    nom[strlen(nom) - 3] = '\0';
    nom_min = cm_lower(nom, strlen(nom));
    cami_min = cm_lower(en->rel, rl - 3);

    {
        sqlite3_stmt *q = id ? s->fitxa_upd : s->fitxa_ins;

        sqlite3_bind_text(q, 1, en->rel, -1, SQLITE_STATIC);
        sqlite3_bind_text(q, 2, en->rel, carpeta_fi ? (int)(carpeta_fi - en->rel) : 0, SQLITE_STATIC);
        sqlite3_bind_text(q, 3, nom, -1, SQLITE_STATIC);
        sqlite3_bind_text(q, 4, nom_min, -1, SQLITE_STATIC);
        sqlite3_bind_text(q, 5, cami_min, -1, SQLITE_STATIC);
        if (tipus)
            sqlite3_bind_text(q, 6, tipus, (int)tipus_l, SQLITE_STATIC);
        if (data)
            sqlite3_bind_text(q, 7, data, (int)data_l, SQLITE_STATIC);
        if (uid)
            sqlite3_bind_text(q, 8, uid, (int)uid_l, SQLITE_STATIC);
        sqlite3_bind_int64(q, 9, (long long)paraules(buf + cos_off, n - cos_off));
        sqlite3_bind_int64(q, 10, en->mtime);
        sqlite3_bind_int64(q, 11, en->mida);
        sqlite3_bind_int64(q, 12, en->ino);
        if (id) {
            sqlite3_bind_int64(q, 13, id);
            treu_fills(s, id);
        }
        r |= pas(q);
        if (!id)
            id = sqlite3_last_insert_rowid(v->db);
        toca(s, id);
        nom_nou(s, nom_min);
    }

    c.s = s;
    c.id = id;
    c.errors = 0;
    c.relacio = NULL;
    c.relacio_len = 0;

    /* propietats, etiquetes i àlies de la capçalera, relacions amb tipus */
    props_txt = xmalloc(pl + 1);
    pl = 0;
    for (size_t i = 0; i < ps.n; i++) {
        const Prop *p = &ps.p[i];

        sqlite3_bind_int64(s->prop, 1, id);
        sqlite3_bind_text(s->prop, 2, p->k, (int)p->kl, SQLITE_STATIC);
        sqlite3_bind_text(s->prop, 3, p->v, (int)p->vl, SQLITE_STATIC);
        sqlite3_bind_int(s->prop, 4, p->idx);
        r |= pas(s->prop);
        memcpy(props_txt + pl, p->v, p->vl);
        pl += p->vl;
        props_txt[pl++] = '\n';

        if (clau(p, "tags") || clau(p, "tag") || clau(p, "etiquetes")) {
            if (p->idx >= 0)
                tag(&c, p->v, p->vl, 0);
            else {                  /* «tags: a, b» o «tags: a b» */
                const char *a = p->v, *e = p->v + p->vl;

                while (a < e) {
                    const char *b = a;

                    while (b < e && *b != ',' && *b != ' ')
                        b++;
                    if (b > a)
                        tag(&c, a, b - a, 0);
                    a = b + 1;
                }
            }
        } else if (clau(p, "aliases") || clau(p, "alias") || clau(p, "alies")) {
            char *pleg = cm_lower(p->v, p->vl);

            sqlite3_bind_int64(s->alies, 1, id);
            sqlite3_bind_text(s->alies, 2, p->v, (int)p->vl, SQLITE_STATIC);
            sqlite3_bind_text(s->alies, 3, pleg, -1, SQLITE_TRANSIENT);
            r |= pas(s->alies);
            nom_nou(s, pleg);
            free(pleg);
        } else {
            c.relacio = p->k;
            c.relacio_len = p->kl;
            cm_scan_value(p->v, p->vl, ref_cb, &c);
            c.relacio = NULL;
        }
    }
    props_txt[pl] = '\0';

    /* cos: enllaços, etiquetes i blocs */
    cm_scan(buf + cos_off, n - cos_off, body_line, ref_cb, &c);
    r |= c.errors ? -1 : 0;

    /* text complet: el punt volat fora, perquè «collecció» trobi «col·lecció» */
    cos_fts = xmalloc(n - cos_off + 1);
    {
        size_t o = 0;

        for (size_t i = cos_off; i < n; i++) {
            if ((unsigned char)buf[i] == 0xC2 && i + 1 < n && (unsigned char)buf[i + 1] == 0xB7) {
                i++;
                continue;
            }
            cos_fts[o++] = buf[i];
        }
        cos_fts[o] = '\0';
        sqlite3_bind_int64(s->text, 1, id);
        sqlite3_bind_text(s->text, 2, nom, -1, SQLITE_STATIC);
        sqlite3_bind_text(s->text, 3, props_txt, (int)pl, SQLITE_STATIC);
        sqlite3_bind_text(s->text, 4, cos_fts, (int)o, SQLITE_STATIC);
        r |= pas(s->text);
    }

    free(cos_fts);
    free(props_txt);
    free(ps.p);
    free(nom);
    free(nom_min);
    free(cami_min);
    free(buf);
    return r;
}

static int resol(Vault *v, int parcial, char *err, size_t errsz)
{
    char sql[2048];

    snprintf(sql, sizeof sql, "%s%s", RESOL, parcial ? RESOL_PARCIAL : "");
    return exec(v, sql, err, errsz);
}

/* ---------- sincronitzar ---------- */

static double ara(void)
{
    struct timespec t;

    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec / 1e9;
}

int vault_sync(Vault *v, int refes, vault_progress_fn fn, void *ud, VaultSync *out,
               char *err, size_t errsz)
{
    Llista fs = { NULL, 0, 0 };
    Sents s;
    sqlite3_stmt *q;
    size_t total = 0, fet = 0;
    VaultSync res = { 0, 0, 0, 0, 0 };
    double t0 = ara();
    int r = 0;

    if (refes && (exec(v, BUIDA, err, errsz) || exec(v, ESQUEMA, err, errsz) ||
                  exec(v, "INSERT INTO meta VALUES('esquema', '" VAULT_SCHEMA "')", err, errsz)))
        return -1;

    recorre(v->root, "", &fs);
    qsort(fs.e, fs.n, sizeof *fs.e, per_cami);

    memset(&s, 0, sizeof s);
    if (prepara(v, &s)) {
        seterr(err, errsz, "índex: %s%s", sqlite3_errmsg(v->db), NULL);
        finalitza(&s);
        allibera(&fs);
        return -1;
    }
    if (exec(v, "BEGIN IMMEDIATE", err, errsz)) {
        finalitza(&s);
        allibera(&fs);
        return -1;
    }

    /* compara amb el que hi ha a l'índex; les esborrades es treuen després */
    {
        long long *morts = NULL;
        size_t nm = 0, cm = 0;

        sqlite3_prepare_v2(v->db, "SELECT id, cami, mtime, mida, ino FROM fitxes", -1, &q, NULL);
        while (sqlite3_step(q) == SQLITE_ROW) {
            Entrada key, *e;

            key.rel = (char *)sqlite3_column_text(q, 1);
            e = bsearch(&key, fs.e, fs.n, sizeof *fs.e, per_cami);
            if (!e) {
                if (nm == cm) {
                    cm = cm ? 2 * cm : 64;
                    morts = xrealloc(morts, cm * sizeof *morts);
                }
                morts[nm++] = sqlite3_column_int64(q, 0);
                continue;
            }
            e->id = sqlite3_column_int64(q, 0);
            if (e->mtime == sqlite3_column_int64(q, 2) && e->mida == sqlite3_column_int64(q, 3) &&
                e->ino == sqlite3_column_int64(q, 4)) {
                e->vist = 1;                /* igual: no cal tornar-la a llegir */
                res.iguals++;
            }
        }
        sqlite3_finalize(q);
        for (size_t i = 0; i < nm; i++) {
            toca(&s, morts[i]);
            treu_fills(&s, morts[i]);
            sqlite3_bind_int64(s.del_fitxa, 1, morts[i]);
            r |= pas(s.del_fitxa);
        }
        res.esborrades = nm;
        free(morts);
    }

    for (size_t i = 0; i < fs.n; i++)
        total += !fs.e[i].vist;
    for (size_t i = 0; i < fs.n && !r; i++) {
        if (fs.e[i].vist)
            continue;
        if (fs.e[i].id)
            res.canviades++;
        else
            res.noves++;
        r |= indexa(v, &s, &fs.e[i]);
        fet++;
        if (fn && (fet % 25 == 0 || fet == total))
            fn(fet, total, ud);
    }
    finalitza(&s);
    if (r) {
        seterr(err, errsz, "índex: %s%s", sqlite3_errmsg(v->db), NULL);
        exec(v, "ROLLBACK", NULL, 0);
        allibera(&fs);
        return -1;
    }
    if ((res.noves || res.canviades || res.esborrades || refes) &&
        resol(v, !refes && res.noves + res.canviades + res.esborrades <= 200, err, errsz)) {
        exec(v, "ROLLBACK", NULL, 0);
        allibera(&fs);
        return -1;
    }
    if (exec(v, "COMMIT", err, errsz)) {
        allibera(&fs);
        return -1;
    }
    if (total > 200)
        exec(v, "INSERT INTO text(text) VALUES('optimize'); PRAGMA wal_checkpoint(TRUNCATE);", NULL, 0);
    allibera(&fs);
    res.segons = ara() - t0;
    if (out)
        *out = res;
    return 0;
}

int vault_reindex(Vault *v, const char *rel, char *err, size_t errsz)
{
    char path[8192];
    struct stat st;
    Sents s;
    sqlite3_stmt *q;
    Llista l = { NULL, 0, 0 };
    long long id = 0;
    int r = 0;

    memset(&s, 0, sizeof s);
    if (prepara(v, &s)) {
        seterr(err, errsz, "índex: %s%s", sqlite3_errmsg(v->db), NULL);
        finalitza(&s);
        return -1;
    }
    sqlite3_prepare_v2(v->db, "SELECT id FROM fitxes WHERE cami=?", -1, &q, NULL);
    sqlite3_bind_text(q, 1, rel, -1, SQLITE_STATIC);
    if (sqlite3_step(q) == SQLITE_ROW)
        id = sqlite3_column_int64(q, 0);
    sqlite3_finalize(q);
    if (exec(v, "BEGIN IMMEDIATE", err, errsz)) {
        finalitza(&s);
        return -1;
    }
    snprintf(path, sizeof path, "%s/%s", v->root, rel);
    if (lstat(path, &st) || !S_ISREG(st.st_mode)) {
        if (id) {
            toca(&s, id);
            treu_fills(&s, id);
            sqlite3_bind_int64(s.del_fitxa, 1, id);
            r |= pas(s.del_fitxa);
        }
    } else {
        afegeix(&l, rel, &st);
        l.e[0].id = id;
        r |= indexa(v, &s, &l.e[0]);
        allibera(&l);
    }
    finalitza(&s);
    if (r || resol(v, 1, err, errsz)) {
        if (r)
            seterr(err, errsz, "índex: %s%s", sqlite3_errmsg(v->db), NULL);
        exec(v, "ROLLBACK", NULL, 0);
        return -1;
    }
    return exec(v, "COMMIT", err, errsz);
}
