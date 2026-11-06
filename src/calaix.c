#include "calaix.h"
#include "config.h"
#include "util.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

/* Peu: «∙ AAAA-MM-DD» (∙ és U+2219) */
static int is_foot(const char *s, size_t n)
{
    static const char date[] = "dddd-dd-dd";
    size_t i;

    if (n < 4 + sizeof date - 1 || memcmp(s, "\xe2\x88\x99 ", 4))
        return 0;
    for (i = 0; date[i]; i++) {
        char c = s[4 + i];

        if (date[i] == 'd' ? !isdigit((unsigned char)c) : c != date[i])
            return 0;
    }
    return 1;
}

/* Longitud de la línia que comença a p, sense el salt de línia. */
static size_t line_len(const char *s, size_t p, size_t len)
{
    const char *nl = memchr(s + p, '\n', len - p);

    return nl ? (size_t)(nl - (s + p)) : len - p;
}

/* Llegeix tot el fitxer. Un fitxer que no existeix és buit (0). */
static int read_all(const char *path, char **data, size_t *len)
{
    FILE *f = fopen(path, "rb");
    char *buf;
    size_t cap = 4096, n = 0;
    int err;

    *data = NULL;
    *len = 0;
    if (!f)
        return errno == ENOENT ? 0 : -1;
    buf = xmalloc(cap + 1);
    for (;;) {
        size_t r;

        if (n == cap) {
            char *tmp = realloc(buf, cap * 2 + 1);

            if (!tmp)
                goto fail;
            buf = tmp;
            cap *= 2;
        }
        r = fread(buf + n, 1, cap - n, f);
        if (r == 0)
            break;
        n += r;
    }
    if (ferror(f))
        goto fail;
    buf[n] = '\0';
    fclose(f);
    *data = buf;
    *len = n;
    return 0;

fail:
    err = errno ? errno : ENOMEM;
    fclose(f);
    free(buf);
    errno = err;
    return -1;
}

int calaix_load(const char *path, Calaix *c)
{
    size_t p, ll, cap = 0;
    int open = 0;

    memset(c, 0, sizeof *c);
    if (read_all(path, &c->text, &c->len) < 0)
        return -1;
    for (p = 0; p < c->len; p += ll + 1) {
        const char *s = c->text + p;

        ll = line_len(c->text, p, c->len);
        if (open) {
            if (is_foot(s, ll)) {
                c->flags[c->n - 1] |= CAL_CLOSED;
                c->end[c->n - 1] = p + ll + (p + ll < c->len);
                open = 0;
            }
            continue;
        }
        if (ll < 3 || memcmp(s, "## ", 3))
            continue;
        if (c->n == cap) {
            cap = cap ? cap * 2 : 16;
            c->head = xrealloc(c->head, cap * sizeof *c->head);
            c->end = xrealloc(c->end, cap * sizeof *c->end);
            c->flags = xrealloc(c->flags, cap);
        }
        c->head[c->n] = p;
        c->end[c->n] = c->len;
        c->flags[c->n] = 0;
        open = 1;
        c->n++;
    }
    if (!c->head) {
        c->head = xmalloc(sizeof *c->head);
        c->end = xmalloc(sizeof *c->end);
        c->flags = xmalloc(1);
    }
    return 0;
}

void calaix_free(Calaix *c)
{
    free(c->text);
    free(c->head);
    free(c->end);
    free(c->flags);
    memset(c, 0, sizeof *c);
}

const char *calaix_head(const Calaix *c, size_t i, size_t *len)
{
    *len = line_len(c->text, c->head[i], c->len);
    return c->text + c->head[i];
}

const char *calaix_title(const Calaix *c, size_t i, size_t *len)
{
    size_t p = c->head[i], hl = line_len(c->text, p, c->len);

    p += hl + 1;
    while (p < c->end[i] && p < c->len && c->text[p] == '\n')
        p++;
    if (p < c->end[i] && p < c->len) {
        size_t ll = line_len(c->text, p, c->len);

        if (ll == 3 && !memcmp(c->text + p, "###", 3)) {
            *len = 0;
            return c->text + p + 3;
        }
        if (ll > 4 && !memcmp(c->text + p, "### ", 4)) {
            *len = ll - 4;
            return c->text + p + 4;
        }
    }
    *len = 0;
    return NULL;
}

size_t calaix_line(const Calaix *c, size_t i)
{
    size_t line = 1;

    for (size_t p = 0; p < c->head[i]; p++)
        if (c->text[p] == '\n')
            line++;
    return line;
}

static int same_ci(char a, char b)
{
    return tolower((unsigned char)a) == tolower((unsigned char)b);
}

int calaix_match(const Calaix *c, size_t i, const char *filter)
{
    size_t m = strlen(filter), s;

    if (m == 0)
        return 1;
    for (s = c->head[i]; s + m <= c->end[i]; s++) {
        size_t k = 0;

        while (k < m && same_ci(c->text[s + k], filter[k]))
            k++;
        if (k == m)
            return 1;
    }
    return 0;
}

/* Constructor de text amb creixement automàtic */
typedef struct {
    char   *s;
    size_t  n, cap;
    int     err;    /* memòria insuficient: el text no és vàlid */
} Str;

static int str_put(Str *b, const char *s, size_t n)
{
    if (b->err)
        return -1;
    if (b->n + n + 1 > b->cap) {
        size_t cap = (b->n + n + 1) * 2;
        char *tmp = realloc(b->s, cap);

        if (!tmp) {
            b->err = 1;
            return -1;
        }
        b->s = tmp;
        b->cap = cap;
    }
    memcpy(b->s + b->n, s, n);
    b->n += n;
    b->s[b->n] = '\0';
    return 0;
}

/* Les etiquetes queden com «#a #b», acceptin «a, b» o «#a #b» */
static int str_tags(Str *b, const char *in)
{
    const char *p = in;

    while (*p) {
        size_t k;

        while (*p == ' ' || *p == '\t' || *p == ',' || *p == '#')
            p++;
        k = strcspn(p, " \t,#");
        if (k) {
            if (b->n && str_put(b, " ", 1) < 0)
                return -1;
            if (str_put(b, "#", 1) < 0 || str_put(b, p, k) < 0)
                return -1;
        }
        p += k;
    }
    return 0;
}

static void set_err(char *err, size_t errsz, const char *msg)
{
    snprintf(err, errsz, "%s", msg);
}

/* Afegeix s a b com una sola línia: els salts i els tabuladors són espais i
 * «·» (el separador de la capçalera) passa a «-». Sense espais als extrems. */
static void put_inline(Str *b, const char *s)
{
    while (*s == ' ' || *s == '\t')
        s++;
    for (size_t k = 0; s[k];) {
        if (!memcmp(s + k, "\xc2\xb7", 2)) {
            str_put(b, "-", 1);
            k += 2;
        } else {
            str_put(b, (s[k] == '\n' || s[k] == '\r' || s[k] == '\t') ? " " : s + k, 1);
            k++;
        }
    }
    while (b->n && b->s[b->n - 1] == ' ')
        b->s[--b->n] = '\0';
}

int calaix_append(const char *path, const char *font, const char *page,
                  const char *tags, const char *title, const char *origin,
                  const char *body, size_t n, char *err, size_t errsz)
{
    static const char heading[] = "# Calaix\n\n";
    char *data, *target, *tmp;
    size_t len, t = 0;
    time_t now = time(NULL);
    struct tm tm;
    char stamp[32];
    struct stat st;
    mode_t mode = 0644;
    Str out = { NULL, 0, 0, 0 };
    FILE *f;
    int fd, rc = -1, parts = 0;

    if (read_all(path, &data, &len) < 0) {
        set_err(err, errsz, strerror(errno));
        return -1;
    }
    while (n && body[n - 1] == '\n')
        n--;
    if (len == 0) {
        str_put(&out, heading, sizeof heading - 1);
    } else {
        /* El fitxer acaba amb una línia en blanc abans de la nova entrada */
        str_put(&out, data, len);
        for (size_t i = out.n; i > 0 && out.s[i - 1] == '\n'; i--)
            t++;
        for (; t < 2; t++)
            str_put(&out, "\n", 1);
    }
    localtime_r(&now, &tm);
    strftime(stamp, sizeof stamp, "%Y-%m-%d", &tm);
    str_put(&out, "## ", 3);
    if (font && *font) {
        put_inline(&out, font);
        parts++;
        if (page && *page) {
            str_put(&out, " \xc2\xb7 p. ", 7);
            put_inline(&out, page);
        }
    }
    if (tags && *tags) {
        Str nt = { NULL, 0, 0, 0 };

        if (str_tags(&nt, tags) == 0 && nt.n) {
            if (parts)
                str_put(&out, " \xc2\xb7 ", 4);
            str_put(&out, nt.s, nt.n);
            parts++;
        }
        free(nt.s);
    }
    if (!parts)
        str_put(&out, "(idea)", 6);
    str_put(&out, "\n\n", 2);
    {
        /* Títol opcional: «### títol». Si no n'hi ha però el text comença per
         * «###», una línia «###» buida evita que el text es llegeixi com a títol. */
        Str tt = { NULL, 0, 0, 0 };

        if (title)
            put_inline(&tt, title);
        if (tt.n) {
            str_put(&out, "### ", 4);
            str_put(&out, tt.s, tt.n);
            str_put(&out, "\n\n", 2);
        } else if (n >= 3 && !memcmp(body, "###", 3) && (n == 3 || body[3] == ' ')) {
            str_put(&out, "###\n\n", 5);
        }
        free(tt.s);
    }
    str_put(&out, body, n);
    str_put(&out, "\n\n\xe2\x88\x99 ", 6);
    str_put(&out, stamp, strlen(stamp));
    if (origin && *origin) {
        str_put(&out, " \xc2\xb7 ", 4);
        put_inline(&out, origin);
    }
    str_put(&out, "\n", 1);
    free(data);
    if (out.err) {
        set_err(err, errsz, "memòria insuficient");
        free(out.s);
        return -1;
    }

    /* Escriptura atòmica al mateix directori, com el desat del document */
    target = realpath(path, NULL);
    if (!target)
        target = xstrdup(path);
    tmp = xmalloc(strlen(target) + 16);
    strcpy(tmp, target);
    strcat(tmp, ".XXXXXX");
    if (stat(target, &st) == 0)
        mode = st.st_mode & 07777;
    if ((fd = mkstemp(tmp)) < 0) {
        set_err(err, errsz, strerror(errno));
        goto out;
    }
    if (fchmod(fd, mode) < 0 || !(f = fdopen(fd, "wb"))) {
        set_err(err, errsz, strerror(errno));
        close(fd);
        unlink(tmp);
        goto out;
    }
    if (fwrite(out.s, 1, out.n, f) != out.n || fflush(f) != 0 || fsync(fileno(f)) != 0 ||
        fclose(f) != 0) {
        set_err(err, errsz, strerror(errno));
        unlink(tmp);
        goto out;
    }
    if (rename(tmp, target) < 0) {
        set_err(err, errsz, strerror(errno));
        unlink(tmp);
        goto out;
    }
    rc = 0;

out:
    free(out.s);
    free(tmp);
    free(target);
    return rc;
}

/* ---- Estat persistent: font activa, pàgina i fonts recents ---- */

static void state_path(char *out, size_t size)
{
    char dir[1024];

    config_dir(dir, sizeof dir);
    snprintf(out, size, "%s/calaix-font", dir);
}

/* Còpia d'una línia dins un camp de mida fixa, sense el salt de línia */
static void copy_field(char *dst, size_t size, const char *src)
{
    size_t k = strcspn(src, "\r\n");

    if (k >= size)
        k = size - 1;
    memcpy(dst, src, k);
    dst[k] = '\0';
}

void calaix_state_load(CalaixState *s)
{
    char path[1100], line[CALAIX_FONT_MAX + 8];
    FILE *f;

    memset(s, 0, sizeof *s);
    state_path(path, sizeof path);
    if (!(f = fopen(path, "r")))
        return;
    /* Línia 1: font activa; línia 2: pàgina; la resta, fonts recents */
    if (fgets(line, sizeof line, f))
        copy_field(s->font, sizeof s->font, line);
    if (fgets(line, sizeof line, f))
        copy_field(s->page, sizeof s->page, line);
    while (s->nrecent < CALAIX_RECENT && fgets(line, sizeof line, f)) {
        copy_field(s->recent[s->nrecent], sizeof s->recent[0], line);
        if (s->recent[s->nrecent][0])
            s->nrecent++;
    }
    fclose(f);
}

int calaix_state_save(const CalaixState *s)
{
    char dir[1024], path[1100], tmp[1110];
    FILE *f;
    int err = 0;

    config_dir(dir, sizeof dir);
    mkdir(dir, 0700);   /* si ja existeix, res */
    state_path(path, sizeof path);
    snprintf(tmp, sizeof tmp, "%s.nou", path);
    if (!(f = fopen(tmp, "w")))
        return -1;
    fprintf(f, "%s\n%s\n", s->font, s->page);
    for (size_t i = 0; i < s->nrecent; i++)
        fprintf(f, "%s\n", s->recent[i]);
    if (fflush(f) != 0 || fsync(fileno(f)) != 0)
        err = errno;
    if (fclose(f) != 0 && !err)
        err = errno;
    if (!err && rename(tmp, path) != 0)
        err = errno;
    if (err) {
        unlink(tmp);
        errno = err;
        return -1;
    }
    return 0;
}

void calaix_state_set_font(CalaixState *s, const char *font)
{
    char name[CALAIX_FONT_MAX];
    size_t at;

    copy_field(name, sizeof name, font);
    for (size_t i = strlen(name); i > 0 && name[i - 1] == ' '; i--)
        name[i - 1] = '\0';
    if (strcmp(name, s->font))
        s->page[0] = '\0';
    memcpy(s->font, name, sizeof name);
    if (!name[0])
        return;
    for (at = 0; at < s->nrecent && strcmp(s->recent[at], name); at++)
        ;
    if (at == s->nrecent) {
        if (s->nrecent < CALAIX_RECENT)
            s->nrecent++;
        at = s->nrecent - 1;
    }
    for (; at > 0; at--)
        memcpy(s->recent[at], s->recent[at - 1], sizeof s->recent[0]);
    memcpy(s->recent[0], name, sizeof name);
}
