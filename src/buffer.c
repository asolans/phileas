#include "buffer.h"
#include "util.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void line_init(Line *l, const char *s, size_t n)
{
    memset(l, 0, sizeof *l);
    l->cap = n + 1 < 16 ? 16 : n + 1;
    l->text = xmalloc(l->cap);
    memcpy(l->text, s, n);
    l->text[n] = '\0';
    l->len = n;
}

static void push_line(Buffer *b, const char *s, size_t n)
{
    if (b->nlines == b->cap) {
        b->cap = b->cap ? b->cap * 2 : 64;
        b->lines = xrealloc(b->lines, b->cap * sizeof *b->lines);
    }
    line_init(&b->lines[b->nlines++], s, n);
}

void buf_init(Buffer *b)
{
    memset(b, 0, sizeof *b);
    b->final_newline = 1;
    push_line(b, "", 0);
}

void buf_free(Buffer *b)
{
    for (size_t i = 0; i < b->nlines; i++) {
        free(b->lines[i].text);
        free(b->lines[i].rows);
    }
    free(b->lines);
    memset(b, 0, sizeof *b);
}

static char *read_file(const char *path, size_t *len)
{
    FILE *f;
    struct stat st;
    char *data;
    size_t cap, n = 0, r;
    int err;

    if (!(f = fopen(path, "rb")))
        return NULL;
    if (fstat(fileno(f), &st) < 0)
        goto fail;
    if (!S_ISREG(st.st_mode)) {
        errno = S_ISDIR(st.st_mode) ? EISDIR : EINVAL;
        goto fail;
    }
    cap = (size_t)st.st_size + 1;
    data = xmalloc(cap);
    while ((r = fread(data + n, 1, cap - n, f)) > 0) {
        n += r;
        if (n == cap)
            data = xrealloc(data, cap *= 2);
    }
    if (ferror(f)) {
        free(data);
        errno = EIO;
        goto fail;
    }
    fclose(f);
    *len = n;
    return data;

fail:
    err = errno;
    fclose(f);
    errno = err;
    return NULL;
}

int buf_load(Buffer *b, const char *path)
{
    Buffer nb;
    size_t n, start = 0, ls, nl = 0, crlf = 0;
    char *data = read_file(path, &n);

    if (!data)
        return -1;
    memset(&nb, 0, sizeof nb);
    if (n >= 3 && memcmp(data, "\xEF\xBB\xBF", 3) == 0) {
        nb.bom = 1;
        start = 3;
    }
    /* Només tractem el fitxer com a CRLF si ho són tots els salts */
    for (size_t i = start; i < n; i++) {
        if (data[i] == '\n') {
            nl++;
            if (i > start && data[i - 1] == '\r')
                crlf++;
        }
    }
    nb.crlf = nl > 0 && crlf == nl;
    nb.final_newline = n > start && data[n - 1] == '\n';

    ls = start;
    for (size_t i = start; i < n; i++) {
        if (data[i] == '\n') {
            push_line(&nb, data + ls, i - ls - (size_t)nb.crlf);
            ls = i + 1;
        }
    }
    if (ls < n || nb.nlines == 0)
        push_line(&nb, data + ls, n - ls);
    free(data);

    buf_free(b);
    *b = nb;
    return 0;
}

/* Desa en un fitxer temporal del mateix directori i el reanomena: si alguna
 * cosa falla a mig camí, el fitxer original queda intacte. */
int buf_save(Buffer *b, const char *path)
{
    char *real = realpath(path, NULL);  /* segueix els enllaços simbòlics */
    const char *target = real ? real : path;
    const char *slash = strrchr(target, '/');
    int dirlen = slash ? (int)(slash - target + 1) : 0;
    const char *eol = b->crlf ? "\r\n" : "\n";
    size_t tlen = strlen(target) + 16;
    char *tmp = xmalloc(tlen);
    struct stat st;
    mode_t mode, mask;
    FILE *f = NULL;
    int fd, err;

    if (stat(target, &st) == 0) {
        mode = st.st_mode & 07777;
    } else {
        mask = umask(0);
        umask(mask);
        mode = 0666 & ~mask;
    }
    snprintf(tmp, tlen, "%.*s.%s.XXXXXX", dirlen, target, target + dirlen);
    if ((fd = mkstemp(tmp)) < 0) {
        err = errno;
        free(tmp);
        free(real);
        errno = err;
        return -1;
    }
    if (fchmod(fd, mode) < 0 || !(f = fdopen(fd, "wb")))
        goto fail;

    if (b->bom)
        fputs("\xEF\xBB\xBF", f);
    for (size_t i = 0; i < b->nlines; i++) {
        fwrite(b->lines[i].text, 1, b->lines[i].len, f);
        if (i + 1 < b->nlines || b->final_newline)
            fputs(eol, f);
    }
    if (fflush(f) != 0 || ferror(f) || fsync(fd) != 0)
        goto fail;
    if (fclose(f) != 0) {
        f = NULL;
        fd = -1;
        goto fail;
    }
    f = NULL;
    fd = -1;
    if (rename(tmp, target) < 0)
        goto fail;

    free(tmp);
    free(real);
    b->modified = 0;
    return 0;

fail:
    err = errno;
    if (f)
        fclose(f);
    else if (fd >= 0)
        close(fd);
    unlink(tmp);
    free(tmp);
    free(real);
    errno = err;
    return -1;
}
