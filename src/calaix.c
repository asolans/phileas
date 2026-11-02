#include "calaix.h"
#include "util.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

/* Capçalera: «## AAAA-MM-DD HH:MM · » (la d és un dígit) */
static int is_head(const char *s, size_t n)
{
    static const char date[] = "dddd-dd-dd dd:dd";
    size_t i;

    if (n < 3 + sizeof date - 1 + 4 || memcmp(s, "## ", 3))
        return 0;
    for (i = 0; date[i]; i++) {
        char c = s[3 + i];

        if (date[i] == 'd' ? !isdigit((unsigned char)c) : c != date[i])
            return 0;
    }
    return !memcmp(s + 3 + i, " \xc2\xb7 ", 4);
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
    size_t p, ll, k;

    memset(c, 0, sizeof *c);
    if (read_all(path, &c->text, &c->len) < 0)
        return -1;
    for (p = 0; p < c->len; p += ll + 1) {
        ll = line_len(c->text, p, c->len);
        if (is_head(c->text + p, ll))
            c->n++;
    }
    c->head = xmalloc((c->n ? c->n : 1) * sizeof *c->head);
    c->end = xmalloc((c->n ? c->n : 1) * sizeof *c->end);
    for (p = 0, k = 0; p < c->len; p += ll + 1) {
        ll = line_len(c->text, p, c->len);
        if (is_head(c->text + p, ll))
            c->head[k++] = p;
    }
    for (k = 0; k < c->n; k++)
        c->end[k] = k + 1 < c->n ? c->head[k + 1] : c->len;
    return 0;
}

void calaix_free(Calaix *c)
{
    free(c->text);
    free(c->head);
    free(c->end);
    memset(c, 0, sizeof *c);
}

const char *calaix_head(const Calaix *c, size_t i, size_t *len)
{
    *len = line_len(c->text, c->head[i], c->len);
    return c->text + c->head[i];
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

int calaix_append(const char *path, const char *origin, const char *tags,
                  const char *body, size_t n, char *err, size_t errsz)
{
    static const char title[] = "# Calaix\n\n";
    char *data, *target, *tmp;
    size_t len, t = 0;
    time_t now = time(NULL);
    struct tm tm;
    char stamp[32];
    struct stat st;
    mode_t mode = 0644;
    Str out = { NULL, 0, 0, 0 };
    FILE *f;
    int fd, rc = -1;

    if (read_all(path, &data, &len) < 0) {
        set_err(err, errsz, strerror(errno));
        return -1;
    }
    while (n && body[n - 1] == '\n')
        n--;
    if (len == 0) {
        str_put(&out, title, sizeof title - 1);
    } else {
        /* El fitxer acaba amb una línia en blanc abans de la nova entrada */
        str_put(&out, data, len);
        for (size_t i = len; i > 0 && data[i - 1] == '\n'; i--)
            t++;
        for (; t < 2; t++)
            str_put(&out, "\n", 1);
    }
    localtime_r(&now, &tm);
    strftime(stamp, sizeof stamp, "%Y-%m-%d %H:%M", &tm);
    str_put(&out, "## ", 3);
    str_put(&out, stamp, strlen(stamp));
    str_put(&out, " \xc2\xb7 ", 4);
    str_put(&out, origin, strlen(origin));
    if (tags && *tags) {
        Str nt = { NULL, 0, 0, 0 };

        if (str_tags(&nt, tags) == 0 && nt.n) {
            str_put(&out, " \xc2\xb7 ", 4);
            str_put(&out, nt.s, nt.n);
        }
        free(nt.s);
    }
    str_put(&out, "\n\n", 2);
    str_put(&out, body, n);
    str_put(&out, "\n\n", 2);
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
