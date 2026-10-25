#include "buffer.h"
#include "utf8.h"
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

/* ---- Edició ----
 * Tota modificació del text passa per buf_insert i buf_delete: és el punt
 * on el desfer/refer (F3) enregistrarà les operacions. */

static void line_changed(Buffer *b, Line *l)
{
    l->wrap_w = 0;
    l->print_lines = 0;
    b->modified = 1;
}

static void line_reserve(Line *l, size_t len)
{
    if (len + 1 > l->cap) {
        while (len + 1 > l->cap)
            l->cap *= 2;
        l->text = xrealloc(l->text, l->cap);
    }
}

static void line_splice(Line *l, size_t at, size_t del, const char *s, size_t n)
{
    line_reserve(l, l->len - del + n);
    memmove(l->text + at + n, l->text + at + del, l->len - at - del + 1);
    memcpy(l->text + at, s, n);
    l->len = l->len - del + n;
}

/* Obre k línies buides (sense inicialitzar) a partir de la posició at. */
static void open_lines(Buffer *b, size_t at, size_t k)
{
    if (b->nlines + k > b->cap) {
        while (b->nlines + k > b->cap)
            b->cap *= 2;
        b->lines = xrealloc(b->lines, b->cap * sizeof *b->lines);
    }
    memmove(b->lines + at + k, b->lines + at, (b->nlines - at) * sizeof *b->lines);
    b->nlines += k;
}

Pos buf_insert(Buffer *b, Pos at, const char *s, size_t n)
{
    Line *l = &b->lines[at.line];
    const char *nl = memchr(s, '\n', n);
    size_t k = 0, tail_len, seg;
    char *tail;
    Pos end;

    if (!nl) {
        line_splice(l, at.col, 0, s, n);
        line_changed(b, l);
        return (Pos){ at.line, at.col + n };
    }

    for (size_t i = 0; i < n; i++)
        k += s[i] == '\n';
    /* El que hi ha darrere del cursor passa al final de l'última línia nova */
    tail_len = l->len - at.col;
    tail = xmalloc(tail_len + 1);
    memcpy(tail, l->text + at.col, tail_len);
    seg = (size_t)(nl - s);
    l->len = at.col;
    l->text[at.col] = '\0';
    line_splice(l, at.col, 0, s, seg);
    line_changed(b, l);

    open_lines(b, at.line + 1, k);
    s = nl + 1;
    n -= seg + 1;
    for (size_t j = 1; j <= k; j++) {
        nl = memchr(s, '\n', n);
        seg = nl ? (size_t)(nl - s) : n;
        line_init(&b->lines[at.line + j], s, seg);
        if (nl) {
            s = nl + 1;
            n -= seg + 1;
        }
    }
    end.line = at.line + k;
    end.col = b->lines[end.line].len;
    line_splice(&b->lines[end.line], end.col, 0, tail, tail_len);
    free(tail);
    return end;
}

void buf_delete(Buffer *b, Pos a, Pos z)
{
    Line *la = &b->lines[a.line], *lz;

    if (a.line == z.line) {
        line_splice(la, a.col, z.col - a.col, "", 0);
        line_changed(b, la);
        return;
    }
    lz = &b->lines[z.line];
    la->len = a.col;
    la->text[a.col] = '\0';
    line_splice(la, a.col, 0, lz->text + z.col, lz->len - z.col);
    line_changed(b, la);
    for (size_t i = a.line + 1; i <= z.line; i++) {
        free(b->lines[i].text);
        free(b->lines[i].rows);
    }
    memmove(b->lines + a.line + 1, b->lines + z.line + 1,
            (b->nlines - z.line - 1) * sizeof *b->lines);
    b->nlines -= z.line - a.line;
}

char *buf_extract(const Buffer *b, Pos a, Pos z, size_t *n)
{
    size_t len = 0, k = 0;
    char *out;

    for (size_t i = a.line; i <= z.line; i++) {
        size_t from = i == a.line ? a.col : 0;
        size_t to = i == z.line ? z.col : b->lines[i].len;
        len += to - from + (i < z.line);
    }
    out = xmalloc(len + 1);
    for (size_t i = a.line; i <= z.line; i++) {
        size_t from = i == a.line ? a.col : 0;
        size_t to = i == z.line ? z.col : b->lines[i].len;
        memcpy(out + k, b->lines[i].text + from, to - from);
        k += to - from;
        if (i < z.line)
            out[k++] = '\n';
    }
    out[k] = '\0';
    *n = k;
    return out;
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

/* A4 amb marges de 2,5 cm (16 x 24,7 cm útils) i una font convencional de
 * 12 punts amb interlineat senzill: unes 80 lletres per línia i 46 línies
 * per pàgina (~600 paraules). Es compta en mitges línies: cada paràgraf
 * ocupa les línies que necessita, una línia en blanc equival a l'espai entre
 * paràgrafs (mitja línia) i una capçalera suma una línia per l'espai que
 * l'envolta. */
#define PRINT_CHARS_PER_LINE 80
#define PRINT_LINES_PER_PAGE 46

static size_t print_half_lines(Line *l)
{
    size_t chars;

    if (l->print_lines == 0) {
        chars = utf8_count(l->text, l->len);
        if (chars == 0) {
            l->print_lines = 1;
        } else {
            l->print_lines = 2 * ((chars + PRINT_CHARS_PER_LINE - 1) / PRINT_CHARS_PER_LINE);
            if (l->text[0] == '#')
                l->print_lines += 2;
        }
    }
    return l->print_lines;
}

size_t buf_pages(Buffer *b)
{
    size_t half = 0, per_page = 2 * PRINT_LINES_PER_PAGE;

    for (size_t i = 0; i < b->nlines; i++)
        half += print_half_lines(&b->lines[i]);
    /* Les línies en blanc del final no omplen cap pàgina */
    for (size_t i = b->nlines; i > 0 && b->lines[i - 1].len == 0; i--)
        half--;
    return half ? (half + per_page - 1) / per_page : 1;
}
