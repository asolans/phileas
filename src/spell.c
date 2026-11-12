#include "spell.h"
#include "md.h"
#include "utf8.h"
#include "util.h"

#include <ctype.h>
#ifndef PHILEAS_HUNSPELL_STATIC
#include <dlfcn.h>
#endif
#include <errno.h>
#include <iconv.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <wctype.h>

/* API C de hunspell (hunspell.h), declarada aquí perquè no calgui el
 * paquet de desenvolupament */
typedef struct Hunhandle Hunhandle;
static struct {
    Hunhandle *(*create)(const char *aff, const char *dic);
    void  (*destroy)(Hunhandle *h);
    int   (*spell)(Hunhandle *h, const char *word);
    char *(*get_dic_encoding)(Hunhandle *h);
    int   (*suggest)(Hunhandle *h, char ***list, const char *word);
    void  (*free_list)(Hunhandle *h, char ***list, int n);
    int   (*add)(Hunhandle *h, const char *word);
} hs;

#ifdef PHILEAS_HUNSPELL_STATIC
/* Executable estàtic (make HUNSPELL_STATIC=1): no hi ha dlopen i la
 * biblioteca va enllaçada */
Hunhandle *Hunspell_create(const char *aff, const char *dic);
void  Hunspell_destroy(Hunhandle *h);
int   Hunspell_spell(Hunhandle *h, const char *word);
char *Hunspell_get_dic_encoding(Hunhandle *h);
int   Hunspell_suggest(Hunhandle *h, char ***list, const char *word);
void  Hunspell_free_list(Hunhandle *h, char ***list, int n);
int   Hunspell_add(Hunhandle *h, const char *word);
#else
static void *lib;
#endif
static char  error[256];

/* ---- Diccionaris carregats ---- */

#define CACHE_SIZE 32768   /* potència de 2; es buida quan és mig plena */
#define MAX_DICTS  2       /* F8 alterna català i castellà sense tornar-los a carregar */

typedef struct {
    char *word;
    int   ok;
} Slot;

/* Cada diccionari té la seva cache de resultats (paraula -> correcta) */
typedef struct {
    Hunhandle    *h;           /* NULL: lloc lliure */
    char          lang[64];
    iconv_t       to, from;    /* si el diccionari no és UTF-8 */
    Slot         *cache;
    size_t        used;
    unsigned long last_use;
} Dict;

static Dict          dicts[MAX_DICTS];
static Dict         *cur;      /* el diccionari actiu */
static unsigned long uses;

/* Paraules ignorades en aquesta sessió (sobreviuen al buidat de la cache) */
static char **ignored;
static size_t nignored;

/* FNV-1a de 64 bits (també en màquines de 32 bits) */
static size_t hash(const char *s, size_t n)
{
    uint64_t h = 14695981039346656037u;

    for (size_t i = 0; i < n; i++)
        h = (h ^ (unsigned char)s[i]) * 1099511628211u;
    return (size_t)(h ^ (h >> 32));
}

static Slot *cache_find(Dict *d, const char *s, size_t n)
{
    size_t i = hash(s, n) & (CACHE_SIZE - 1);

    while (d->cache[i].word &&
           (strlen(d->cache[i].word) != n || memcmp(d->cache[i].word, s, n)))
        i = (i + 1) & (CACHE_SIZE - 1);
    return &d->cache[i];
}

static void cache_clear(Dict *d)
{
    for (size_t i = 0; d->cache && i < CACHE_SIZE; i++) {
        free(d->cache[i].word);
        d->cache[i].word = NULL;
    }
    d->used = 0;
}

static void cache_put(Dict *d, const char *s, size_t n, int ok)
{
    Slot *sl;

    if (d->used >= CACHE_SIZE / 2) {
        cache_clear(d);
        for (size_t i = 0; i < nignored; i++)
            cache_put(d, ignored[i], strlen(ignored[i]), 1);
    }
    sl = cache_find(d, s, n);
    if (!sl->word) {
        sl->word = xmalloc(n + 1);
        memcpy(sl->word, s, n);
        sl->word[n] = '\0';
        d->used++;
    }
    sl->ok = ok;
}

static void dict_free(Dict *d)
{
    if (d->h) {
        hs.destroy(d->h);
        if (d->to != (iconv_t)-1)
            iconv_close(d->to);
        if (d->from != (iconv_t)-1)
            iconv_close(d->from);
    }
    cache_clear(d);
    free(d->cache);
    memset(d, 0, sizeof *d);
    if (cur == d)
        cur = NULL;
}

/* ---- Càrrega ---- */

#ifdef PHILEAS_HUNSPELL_STATIC
static int load_lib(void)
{
    hs.create = Hunspell_create;
    hs.destroy = Hunspell_destroy;
    hs.spell = Hunspell_spell;
    hs.get_dic_encoding = Hunspell_get_dic_encoding;
    hs.suggest = Hunspell_suggest;
    hs.free_list = Hunspell_free_list;
    hs.add = Hunspell_add;
    return 0;
}
#else
static int load_lib(void)
{
    static const char *names[] = { "libhunspell-1.7.so.0", "libhunspell-1.7.so",
                                   "libhunspell.so", "libhunspell-1.6.so.0" };

    if (lib)
        return 0;
    for (size_t i = 0; i < sizeof names / sizeof *names && !lib; i++)
        lib = dlopen(names[i], RTLD_NOW | RTLD_LOCAL);
    if (!lib) {
        snprintf(error, sizeof error, "no s'ha trobat la biblioteca hunspell");
        return -1;
    }
#define SYM(field, name) \
    if (!(*(void **)&hs.field = dlsym(lib, name))) goto bad
    SYM(create, "Hunspell_create");
    SYM(destroy, "Hunspell_destroy");
    SYM(spell, "Hunspell_spell");
    SYM(get_dic_encoding, "Hunspell_get_dic_encoding");
    SYM(suggest, "Hunspell_suggest");
    SYM(free_list, "Hunspell_free_list");
    SYM(add, "Hunspell_add");
#undef SYM
    return 0;
bad:
    snprintf(error, sizeof error, "la biblioteca hunspell no és compatible");
    dlclose(lib);
    lib = NULL;
    return -1;
}
#endif

static int exists(const char *path)
{
    struct stat st;

    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

/* Busca name.aff i name.dic als directoris habituals. */
static int find_dict(const char *name, char *aff, char *dic, size_t size)
{
    static const char *dirs[] = { "/usr/share/hunspell", "/usr/share/myspell",
                                  "/usr/share/myspell/dicts", "/usr/local/share/hunspell" };
    const char *env = getenv("DICPATH"), *home = getenv("HOME");
    char dir[1024];

    if (strchr(name, '/'))
        return 0;
    /* $DICPATH (separat per ':'), després ~/.local/share/hunspell */
    for (const char *p = env; p && *p;) {
        size_t n = strcspn(p, ":");

        snprintf(dir, sizeof dir, "%.*s", (int)n, p);
        snprintf(aff, size, "%s/%s.aff", dir, name);
        snprintf(dic, size, "%s/%s.dic", dir, name);
        if (exists(aff) && exists(dic))
            return 1;
        p += n + (p[n] == ':');
    }
    if (home) {
        snprintf(aff, size, "%s/.local/share/hunspell/%s.aff", home, name);
        snprintf(dic, size, "%s/.local/share/hunspell/%s.dic", home, name);
        if (exists(aff) && exists(dic))
            return 1;
    }
    for (size_t i = 0; i < sizeof dirs / sizeof *dirs; i++) {
        snprintf(aff, size, "%s/%s.aff", dirs[i], name);
        snprintf(dic, size, "%s/%s.dic", dirs[i], name);
        if (exists(aff) && exists(dic))
            return 1;
    }
    return 0;
}

void spell_code(const char *name, char *out, size_t size)
{
    const char *env = name;

    if (!env) {
        env = getenv("LC_ALL");
        if (!env || !*env)
            env = getenv("LC_MESSAGES");
        if (!env || !*env)
            env = getenv("LANG");
        if (!env || !*env || !strcmp(env, "C") || !strcmp(env, "POSIX") || !strncmp(env, "C.", 2))
            env = "ca";
    }
    snprintf(out, size, "%.*s", (int)strcspn(env, "_-.@"), env);
}

/* Un diccionari personal per idioma, al directori de configuració:
 * diccionari-ca.txt, diccionari-es.txt... */
static void personal_path(char *out, size_t size)
{
    char dir[900], code[32];

    config_dir(dir, sizeof dir);
    spell_code(cur ? cur->lang : NULL, code, sizeof code);
    snprintf(out, size, "%s/diccionari-%s.txt", dir, code);
}

/* Converteix entre UTF-8 i la codificació del diccionari. Retorna una còpia
 * nova (cal alliberar-la) o NULL si no es pot representar. */
static char *convert(iconv_t cd, const char *s, size_t n)
{
    size_t outsz = n * 4 + 4, left = outsz - 1, inleft = n;
    char *out = xmalloc(outsz), *op = out, *ip = (char *)s;

    if (cd == (iconv_t)-1) {
        memcpy(out, s, n);
        out[n] = '\0';
        return out;
    }
    iconv(cd, NULL, NULL, NULL, NULL);
    if (iconv(cd, &ip, &inleft, &op, &left) == (size_t)-1) {
        free(out);
        return NULL;
    }
    *op = '\0';
    return out;
}

/* Afegeix al diccionari d les paraules del fitxer (una per línia) */
static void load_words_file(Dict *d, const char *path)
{
    char line[256];
    FILE *f = fopen(path, "r");

    if (!f)
        return;
    while (fgets(line, sizeof line, f)) {
        size_t n = strcspn(line, "\r\n");
        char *w;

        if (n == 0)
            continue;
        if ((w = convert(d->to, line, n))) {
            hs.add(d->h, w);
            free(w);
        }
        cache_put(d, line, n, 1);
    }
    fclose(f);
}

/* Diccionari de projecte: .diccionari.txt a la carpeta del document o a una
 * de superior; serveix per a tots els idiomes (noms propis d'una novel·la) */
static char project_file[1024];

static void load_personal(void)
{
    char path[1024];

    personal_path(path, sizeof path);
    load_words_file(cur, path);
    if (project_file[0])
        load_words_file(cur, project_file);
}

static Dict *find_loaded(const char *name)
{
    for (int i = 0; i < MAX_DICTS; i++)
        if (dicts[i].h && !strcmp(dicts[i].lang, name))
            return &dicts[i];
    return NULL;
}

static int open_one(const char *name)
{
    char aff[1100], dic[1100];
    Hunhandle *h;
    const char *enc;
    Dict *d;

    if ((d = find_loaded(name))) {
        cur = d;
        d->last_use = ++uses;
        return 0;
    }
    if (!find_dict(name, aff, dic, sizeof aff))
        return -1;
    if (!(h = hs.create(aff, dic)))
        return -1;
    /* Un lloc lliure, o el diccionari que fa més temps que no es fa servir */
    d = &dicts[0];
    for (int i = 0; i < MAX_DICTS; i++) {
        if (!dicts[i].h) {
            d = &dicts[i];
            break;
        }
        if (dicts[i].last_use < d->last_use)
            d = &dicts[i];
    }
    dict_free(d);
    d->h = h;
    d->to = d->from = (iconv_t)-1;
    d->cache = calloc(CACHE_SIZE, sizeof *d->cache);
    if (!d->cache)
        die("sense memòria");
    for (size_t i = 0; i < nignored; i++)
        cache_put(d, ignored[i], strlen(ignored[i]), 1);
    snprintf(d->lang, sizeof d->lang, "%s", name);
    d->last_use = ++uses;
    enc = hs.get_dic_encoding(h);
    if (enc && strcmp(enc, "UTF-8") && strcmp(enc, "utf-8")) {
        d->to = iconv_open(enc, "UTF-8");
        d->from = iconv_open("UTF-8", enc);
    }
    cur = d;
    load_personal();
    return 0;
}

/* "es" -> "es_ES", "fr" -> "fr_FR"; 0 si name ja té país o variant. */
static int full_name(const char *name, char *out, size_t size)
{
    if (strpbrk(name, "_-") || strlen(name) >= 8)
        return 0;
    snprintf(out, size, "%s_%s", name, name);
    for (char *p = out + strlen(name) + 1; *p; p++)
        *p = (char)toupper((unsigned char)*p);
    return 1;
}

int spell_loaded(const char *name)
{
    char full[64];

    return name && (find_loaded(name) || (full_name(name, full, sizeof full) && find_loaded(full)));
}

int spell_open(const char *name)
{
    char loc[64];
    const char *env;

    if (load_lib() < 0)
        return -1;
    if (name) {
        char full[64];

        /* Primer el que ja hi ha carregat, sense buscar fitxers */
        if (full_name(name, full, sizeof full) && !find_loaded(name) && find_loaded(full))
            return open_one(full);
        if (open_one(name) == 0)
            return 0;
        /* Només l'idioma: "es" -> "es_ES" */
        if (full_name(name, full, sizeof full) && open_one(full) == 0)
            return 0;
        snprintf(error, sizeof error, "no hi ha cap diccionari «%s»", name);
        return -1;
    }
    /* De la configuració regional: ca_ES.UTF-8 -> ca_ES -> ca */
    env = getenv("LC_ALL");
    if (!env || !*env)
        env = getenv("LC_MESSAGES");
    if (!env || !*env)
        env = getenv("LANG");
    if (env && *env && strcmp(env, "C") && strcmp(env, "POSIX")) {
        snprintf(loc, sizeof loc, "%.*s", (int)strcspn(env, ".@"), env);
        if (open_one(loc) == 0)
            return 0;
        loc[strcspn(loc, "_")] = '\0';
        if (open_one(loc) == 0)
            return 0;
    }
    if (open_one("ca") == 0)
        return 0;
    snprintf(error, sizeof error, "no s'ha trobat cap diccionari");
    return -1;
}

void spell_close(void)
{
    for (int i = 0; i < MAX_DICTS; i++)
        dict_free(&dicts[i]);
    cur = NULL;
}

const char *spell_lang(void)
{
    return cur ? cur->lang : NULL;
}

const char *spell_error(void)
{
    return error;
}

/* ---- Consultes ---- */

int spell_check(const char *s, size_t n)
{
    Slot *sl;
    char *w;
    int ok;

    if (!cur || n == 0)
        return 1;
    sl = cache_find(cur, s, n);
    if (sl->word)
        return sl->ok;
    w = convert(cur->to, s, n);
    ok = !w || hs.spell(cur->h, w) != 0;   /* el que no es pot convertir no es marca */
    free(w);
    cache_put(cur, s, n, ok);
    return ok;
}

int spell_suggest(const char *s, size_t n, char ***list)
{
    char **raw = NULL, *w;
    int k, out = 0;

    *list = NULL;
    if (!cur || !(w = convert(cur->to, s, n)))
        return 0;
    k = hs.suggest(cur->h, &raw, w);
    free(w);
    if (k <= 0)
        return 0;
    *list = xmalloc((size_t)k * sizeof **list);
    for (int i = 0; i < k; i++) {
        char *u = convert(cur->from, raw[i], strlen(raw[i]));

        if (u)
            (*list)[out++] = u;
    }
    hs.free_list(cur->h, &raw, k);
    return out;
}

void spell_free_list(char **list, int n)
{
    for (int i = 0; i < n; i++)
        free(list[i]);
    free(list);
}

static int mkdir_parents(char *path)
{
    for (char *p = path + 1; *p; p++) {
        if (*p != '/')
            continue;
        *p = '\0';
        if (mkdir(path, 0755) < 0 && errno != EEXIST) {
            *p = '/';
            return -1;
        }
        *p = '/';
    }
    return 0;
}

int spell_add(const char *s, size_t n)
{
    char path[1024], *w;
    FILE *f;

    if (!cur)
        return 0;
    if ((w = convert(cur->to, s, n))) {
        hs.add(cur->h, w);
        free(w);
    }
    cache_put(cur, s, n, 1);
    personal_path(path, sizeof path);
    if (mkdir_parents(path) < 0 || !(f = fopen(path, "a")))
        return -1;
    fprintf(f, "%.*s\n", (int)n, s);
    return fclose(f) == 0 ? 0 : -1;
}

/* ---- Diccionari de projecte ---- */

#define PROJECT_DICT ".diccionari.txt"
#define PROJECT_LEVELS 6

/* Carpeta del document (ruta absoluta), o 0 */
static int doc_dir(const char *docpath, char *dir, size_t sz)
{
    char tmp[1024], *slash;

    if (!docpath || !*docpath || strlen(docpath) >= sizeof tmp)
        return 0;
    snprintf(tmp, sizeof tmp, "%s", docpath);
    slash = strrchr(tmp, '/');
    if (slash && slash != tmp)
        *slash = '\0';
    else if (slash)
        slash[1] = '\0';
    else
        snprintf(tmp, sizeof tmp, ".");
    return realpath(tmp, dir) != NULL && strlen(dir) + strlen(PROJECT_DICT) + 2 < sz;
}

static int file_exists(const char *path)
{
    struct stat st;

    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

/* El diccionari de projecte que ja existeix per a aquest document, o 0 */
static int project_lookup(const char *docpath, char *out, size_t sz)
{
    char dir[1024];

    if (!doc_dir(docpath, dir, sizeof dir))
        return 0;
    for (int lv = 0; lv < PROJECT_LEVELS; lv++) {
        char *slash;

        if (snprintf(out, sz, "%s/%s", strcmp(dir, "/") ? dir : "", PROJECT_DICT) >= (int)sz)
            break;
        if (file_exists(out))
            return 1;
        if (!strcmp(dir, "/") || !(slash = strrchr(dir, '/')))
            break;
        if (slash == dir)
            dir[1] = '\0';
        else
            *slash = '\0';
    }
    return 0;
}

/* On crear-lo si no n'hi ha: l'arrel d'un projecte de Fàbula (la carpeta amb
 * projecte.md) o, si no, la carpeta del document */
static int project_create_path(const char *docpath, char *out, size_t sz)
{
    char dir[1024], probe[1100], base[1024];

    if (!doc_dir(docpath, dir, sizeof dir))
        return 0;
    snprintf(base, sizeof base, "%s", dir);
    for (int lv = 0; lv < PROJECT_LEVELS; lv++) {
        char *slash;

        snprintf(probe, sizeof probe, "%s/projecte.md", strcmp(dir, "/") ? dir : "");
        if (file_exists(probe)) {
            snprintf(base, sizeof base, "%s", dir);
            break;
        }
        if (!strcmp(dir, "/") || !(slash = strrchr(dir, '/')))
            break;
        if (slash == dir)
            dir[1] = '\0';
        else
            *slash = '\0';
    }
    return snprintf(out, sz, "%s/%s", strcmp(base, "/") ? base : "", PROJECT_DICT) < (int)sz;
}

void spell_project_set(const char *docpath)
{
    char path[1024];

    if (!project_lookup(docpath, path, sizeof path)) {
        project_file[0] = '\0';
        return;
    }
    if (!strcmp(path, project_file))
        return;
    snprintf(project_file, sizeof project_file, "%s", path);
    for (int i = 0; i < MAX_DICTS; i++)
        if (dicts[i].h)
            load_words_file(&dicts[i], project_file);
}

const char *spell_project_file(void)
{
    return project_file[0] ? project_file : NULL;
}

int spell_add_project(const char *s, size_t n, const char *docpath)
{
    char path[1024];
    FILE *f;

    if (!project_file[0]) {
        if (!project_create_path(docpath, path, sizeof path)) {
            errno = ENOENT;
            return -1;
        }
        snprintf(project_file, sizeof project_file, "%s", path);
    }
    if (!(f = fopen(project_file, "a")))
        return -1;
    fprintf(f, "%.*s\n", (int)n, s);
    if (fclose(f) != 0)
        return -1;
    for (int i = 0; i < MAX_DICTS; i++)
        if (dicts[i].h) {
            char *w = convert(dicts[i].to, s, n);

            if (w) {
                hs.add(dicts[i].h, w);
                free(w);
            }
            cache_put(&dicts[i], s, n, 1);
        }
    return 0;
}

void spell_ignore(const char *s, size_t n)
{
    char *w = xmalloc(n + 1);

    memcpy(w, s, n);
    w[n] = '\0';
    ignored = xrealloc(ignored, (nignored + 1) * sizeof *ignored);
    ignored[nignored++] = w;
    for (int i = 0; i < MAX_DICTS; i++)
        if (dicts[i].h)
            cache_put(&dicts[i], s, n, 1);
}

/* ---- Paraules ---- */

static int is_letter(wchar_t wc)
{
    return wc != UTF8_INVALID && iswalpha((wint_t)wc);
}

/* Caràcters que poden anar dins d'una paraula, entre lletres */
static int is_joiner(wchar_t wc)
{
    return wc == L'\'' || wc == L'’' || wc == L'·' || wc == L'-';
}

static wchar_t char_at(const char *s, size_t len, size_t i, size_t *n)
{
    wchar_t wc;

    *n = utf8_decode(s, len, i, &wc);
    return wc;
}

/* La paraula [a, b) forma part d'una adreça o d'un nom de fitxer? Es mira
 * el tros de text sense espais que la conté. */
static int in_address(const char *s, size_t len, size_t a, size_t b)
{
    size_t i = a, j = b;

    while (i > 0 && s[i - 1] != ' ' && s[i - 1] != '\t')
        i--;
    while (j < len && s[j] != ' ' && s[j] != '\t')
        j++;
    for (size_t k = i; k < j; k++) {
        if (s[k] == '@' || (k + 2 < j && !memcmp(s + k, "://", 3)) ||
            (k + 3 < j && !memcmp(s + k, "www.", 4)))
            return 1;
        /* "fitxer.md", "exemple.cat": un punt enganxat a lletres pels dos costats */
        if (s[k] == '.' && k > i && k + 1 < j && (unsigned char)s[k - 1] > ' ' &&
            s[k - 1] != '.' && ((s[k + 1] >= 'a' && s[k + 1] <= 'z') ||
                                (s[k + 1] >= 'A' && s[k + 1] <= 'Z') ||
                                (s[k + 1] >= '0' && s[k + 1] <= '9')))
            return 1;
    }
    return 0;
}

int spell_next_word(const char *s, size_t len, size_t from, size_t *a, size_t *b)
{
    size_t i = from, n;

    while (i < len) {
        size_t start, end, m;
        int digits = 0;
        wchar_t wc = char_at(s, len, i, &n);

        if (!is_letter(wc)) {
            i += n;
            continue;
        }
        start = i;
        end = i += n;
        for (;;) {
            if (i >= len)
                break;
            wc = char_at(s, len, i, &n);
            if (is_letter(wc) || (wc >= L'0' && wc <= L'9') || utf8_width(wc, 0) == 0) {
                digits |= wc >= L'0' && wc <= L'9';
                end = i += n;
            } else if (is_joiner(wc) && i + n < len && is_letter(char_at(s, len, i + n, &m))) {
                end = i += n;
            } else {
                break;
            }
        }
        /* Xifres just abans de la paraula: "2a", "10è" */
        if (start > 0 && s[start - 1] >= '0' && s[start - 1] <= '9')
            digits = 1;
        if (digits || in_address(s, len, start, end))
            continue;
        *a = start;
        *b = end;
        return 1;
    }
    return 0;
}

int spell_next_error(const char *s, size_t len, const unsigned char *styles,
                     size_t from, size_t *a, size_t *b)
{
    size_t x, y;

    while (spell_next_word(s, len, from, &x, &y)) {
        int skip = 0;

        for (size_t i = x; i < y && !skip; i++)
            skip = styles && (styles[i] & (MS_CODE | MS_MARK));
        if (!skip && !spell_check(s + x, y - x)) {
            *a = x;
            *b = y;
            return 1;
        }
        from = y;
    }
    return 0;
}
