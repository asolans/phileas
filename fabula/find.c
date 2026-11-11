#include "find.h"

#include "md.h"
#include "utf8.h"
#include "util.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wctype.h>

static char *read_whole(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    char *buf = NULL;
    size_t n = 0, cap = 0;

    if (!f)
        return NULL;
    for (;;) {
        size_t r;

        if (n + 4096 + 1 > cap)
            buf = xrealloc(buf, cap = cap ? cap * 2 : 8192);
        r = fread(buf + n, 1, cap - n - 1, f);
        n += r;
        if (r == 0)
            break;
    }
    fclose(f);
    buf[n] = '\0';
    *len = n;
    return buf;
}

typedef struct {
    size_t start, end;     /* bytes de la línia, sense el salt */
    size_t bufline;        /* línia del buffer, o (size_t)-1 si és en blanc */
    size_t scene;
    int    meta;           /* capçalera YAML o comentari de metadades */
} RawLine;

static int is_comment(const char *s, size_t len)
{
    size_t i = 0, e = len;

    while (i < e && (s[i] == ' ' || s[i] == '\t'))
        i++;
    while (e > i && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r'))
        e--;
    return e - i >= 7 && !strncmp(s + i, "<!--", 4) && !strncmp(s + e - 3, "-->", 3);
}

/* Les línies del fitxer, amb la seva línia del buffer de Phileas (replicant
 * com les agrupa buf_load), l'escena i si són metadades. */
static size_t split_lines(const char *text, size_t len, RawLine **out)
{
    RawLine *v = NULL;
    size_t n = 0, cap = 0, pos = 0, nlines = 0, blanks = 0, scene_idx = 0, cur_scene = 0;
    int prevk = -1, in_fm = 0, fm_done = 0, fence = 0, prev_heading = 0;
    MdState st;
    int crlf = 0;
    size_t nl = 0, ncr = 0;

    memset(&st, 0, sizeof st);
    if (len >= 3 && !memcmp(text, "\xEF\xBB\xBF", 3))
        pos = 3;
    for (size_t i = pos; i < len; i++)
        if (text[i] == '\n') {
            nl++;
            if (i > pos && text[i - 1] == '\r')
                ncr++;
        }
    crlf = nl > 0 && ncr == nl;
    for (size_t ls = pos, i = pos; i <= len; i++) {
        size_t le, tl;
        const char *s;
        RawLine *r;
        int k;

        if (i < len && text[i] != '\n')
            continue;
        if (i == len && ls >= len)
            break;
        le = i;
        s = text + ls;
        tl = le - ls - (crlf && le > ls && text[le - 1] == '\r' ? 1 : 0);
        if (n == cap)
            v = xrealloc(v, (cap = cap ? cap * 2 : 256) * sizeof *v);
        r = &v[n++];
        r->start = ls;
        r->end = le - (crlf && le > ls && text[le - 1] == '\r' ? 1 : 0);
        r->meta = 0;
        r->bufline = (size_t)-1;

        /* metadades: la capçalera YAML i el comentari sota un títol */
        if (!fm_done && n == 1 && tl == 3 && !memcmp(s, "---", 3))
            in_fm = 1, r->meta = 1;
        else if (in_fm) {
            r->meta = 1;
            if (tl == 3 && (!memcmp(s, "---", 3) || !memcmp(s, "...", 3)))
                in_fm = 0, fm_done = 1;
        } else {
            fm_done = 1;
            if (prev_heading && is_comment(s, tl))
                r->meta = 1;
        }

        /* escena i títols */
        {
            size_t sp = 0;

            while (sp < tl && sp < 3 && s[sp] == ' ')
                sp++;
            if (!in_fm && tl - sp >= 3 && (!memcmp(s + sp, "```", 3) || !memcmp(s + sp, "~~~", 3)))
                fence = !fence;
            prev_heading = 0;
            if (!fence && !r->meta && tl > sp && s[sp] == '#') {
                size_t lv = 0;

                while (sp + lv < tl && s[sp + lv] == '#')
                    lv++;
                if (lv <= 6 && (sp + lv == tl || s[sp + lv] == ' ' || s[sp + lv] == '\t')) {
                    prev_heading = 1;
                    if (lv == 3)
                        cur_scene = ++scene_idx;
                    else if (lv < 3)
                        cur_scene = 0;
                }
            }
        }
        r->scene = cur_scene;

        /* línia del buffer (com buf_load) */
        k = md_kind(s, tl, &st);
        if (k == MD_EMPTY) {
            blanks++;
        } else {
            if (nlines == 0) {
                nlines += blanks;
                blanks = 0;
            } else if (blanks > 0) {
                if (!md_tight(prevk, k))
                    blanks--;
                nlines += blanks;
                blanks = 0;
            } else if (md_joinable(prevk, k)) {
                r->bufline = nlines - 1;
                prevk = MD_BASE(prevk) | (k & MD_HARDBREAK);
                ls = i + 1;
                continue;
            }
            r->bufline = nlines++;
            prevk = k;
        }
        ls = i + 1;
    }
    *out = v;
    return n;
}

static int word_char(const char *s, size_t len, size_t pos, int before)
{
    wchar_t c;
    size_t at;

    if (before) {
        if (pos == 0)
            return 0;
        at = utf8_prev(s, pos);
    } else {
        if (pos >= len)
            return 0;
        at = pos;
    }
    if (utf8_decode(s, len, at, &c) == 0 || c == UTF8_INVALID)
        return 0;
    return iswalnum((wint_t)c) != 0;
}

/* Fragment al voltant de [a, b) amb la coincidència entre « i » */
static char *hit_context(const char *s, size_t len, size_t a, size_t b)
{
    size_t from = a, to = b, o = 0;
    int before = 0, after = 0;
    char *out;

    while (from > 0 && before < 30) {
        from = utf8_prev(s, from);
        before++;
    }
    while (to < len && after < 40) {
        size_t nx = utf8_next(s, len, to);

        to = nx > to ? nx : to + 1;
        after++;
    }
    out = xmalloc((to - from) + 24);
    if (from > 0)
        o += (size_t)sprintf(out + o, "…");
    memcpy(out + o, s + from, a - from);
    o += a - from;
    o += (size_t)sprintf(out + o, "«");
    memcpy(out + o, s + a, b - a);
    o += b - a;
    o += (size_t)sprintf(out + o, "»");
    memcpy(out + o, s + b, to - b);
    o += to - b;
    if (to < len)
        o += (size_t)sprintf(out + o, "…");
    out[o] = '\0';
    return out;
}

static void add_hit(FFind *f, FNode *file, size_t start, size_t end, size_t line, size_t scene,
                    char *ctx, size_t *cap)
{
    if (f->n == *cap)
        f->hits = xrealloc(f->hits, (*cap = *cap ? *cap * 2 : 64) * sizeof *f->hits);
    f->hits[f->n].file = file;
    f->hits[f->n].start = start;
    f->hits[f->n].end = end;
    f->hits[f->n].line = line;
    f->hits[f->n].scene = scene;
    f->hits[f->n].context = ctx;
    f->n++;
}

static void scan_file(FNode *file, const char *text, size_t len, const char *needle, size_t nl,
                      const char *fneedle, size_t fnl, unsigned opts, FFind *f, size_t *cap, FoldBuf *fb)
{
    RawLine *lines;
    size_t n = split_lines(text, len, &lines), before = f->n;

    for (size_t r = 0; r < n; r++) {
        const char *s = text + lines[r].start;
        size_t sl = lines[r].end - lines[r].start, from = 0;

        if (lines[r].meta && !(opts & FF_META))
            continue;
        if (sl < nl)
            continue;
        if (opts & FF_ICASE) {
            fk_fold_line(fb, s, sl);
            while (from + fnl <= fb->n) {
                const char *hit = memmem(fb->f + from, fb->n - from, fneedle, fnl);
                size_t fi, a, b;

                if (!hit)
                    break;
                fi = (size_t)(hit - fb->f);
                a = fb->s[fi];
                b = fb->e[fi + fnl - 1];
                from = fi + 1;
                while (from < fb->n && fb->s[from] == fb->s[fi])      /* no comença a mig caràcter */
                    from++;
                if (fi > 0 && fb->s[fi] == fb->s[fi - 1])             /* a mig d'un caràcter plegat */
                    continue;
                if ((opts & FF_WORD) && (word_char(s, sl, a, 1) || word_char(s, sl, b, 0)))
                    continue;
                if (lines[r].bufline == (size_t)-1)
                    continue;
                add_hit(f, file, lines[r].start + a, lines[r].start + b, lines[r].bufline,
                        lines[r].scene, hit_context(s, sl, a, b), cap);
                from = fi + fnl;
                while (from < fb->n && fb->s[from] == fb->s[fi + fnl - 1])
                    from++;
            }
        } else {
            while (from + nl <= sl) {
                const char *hit = memmem(s + from, sl - from, needle, nl);
                size_t a, b;

                if (!hit)
                    break;
                a = (size_t)(hit - s);
                b = a + nl;
                from = a + 1;
                if ((opts & FF_WORD) && (word_char(s, sl, a, 1) || word_char(s, sl, b, 0)))
                    continue;
                if (lines[r].bufline == (size_t)-1)
                    continue;
                add_hit(f, file, lines[r].start + a, lines[r].start + b, lines[r].bufline,
                        lines[r].scene, hit_context(s, sl, a, b), cap);
                from = b;
            }
        }
    }
    if (f->n > before)
        f->files++;
    free(lines);
}

int find_in_project(const Project *p, const char *needle, unsigned opts, FFind *out, char *err,
                    size_t esz)
{
    FoldBuf fb, nb;
    size_t nl = strlen(needle), cap = 0;

    memset(out, 0, sizeof *out);
    memset(&fb, 0, sizeof fb);
    memset(&nb, 0, sizeof nb);
    if (!nl) {
        snprintf(err, esz, "El text a cercar és buit.");
        return -1;
    }
    if (memchr(needle, '\n', nl) || memchr(needle, '\r', nl)) {
        snprintf(err, esz, "El text a cercar no pot tenir salts de línia.");
        return -1;
    }
    if (opts & FF_ICASE)
        fk_fold_line(&nb, needle, nl);
    if (!p->top)
        return 0;
    for (int pass = 0; pass < 2; pass++)                  /* primer el manuscrit */
        for (size_t g = 0; g < p->top->nkids; g++) {
            FNode *grp = p->top->kids[g];

            if ((pass == 0) != (grp == p->manuscrit))
                continue;
            for (size_t i = 0; i < grp->nkids; i++) {
                FNode *file = grp->kids[i];
                size_t len;
                char *text = read_whole(file->path, &len);

                if (!text)
                    continue;
                scan_file(file, text, len, needle, nl, (opts & FF_ICASE) ? nb.f : NULL,
                          (opts & FF_ICASE) ? nb.n : 0, opts, out, &cap, &fb);
                free(text);
            }
        }
    fk_foldbuf_free(&fb);
    fk_foldbuf_free(&nb);
    return 0;
}

void find_free(FFind *f)
{
    for (size_t i = 0; i < f->n; i++)
        free(f->hits[i].context);
    free(f->hits);
    memset(f, 0, sizeof *f);
}

static int write_file_atomic(const char *path, const char *data, size_t len)
{
    struct stat st;
    size_t pl;
    char *tmp;
    FILE *fp;
    int ok = 0;

    if (!path)
        return -1;
    pl = strlen(path);
    tmp = xmalloc(pl + 40);

    snprintf(tmp, pl + 40, "%s.fabula-tmp.%ld", path, (long)getpid());
    fp = fopen(tmp, "wb");
    if (!fp) {
        free(tmp);
        return -1;
    }
    if (stat(path, &st) == 0)
        (void)chmod(tmp, st.st_mode & 07777);
    if (fwrite(data, 1, len, fp) == len && fflush(fp) == 0 && fsync(fileno(fp)) == 0 && fclose(fp) == 0) {
        fp = NULL;
        ok = rename(tmp, path) == 0;
    }
    if (fp)
        fclose(fp);
    if (!ok) {
        int e = errno;

        unlink(tmp);
        errno = e;
    }
    free(tmp);
    return ok ? 0 : -1;
}

int find_replace(Project *p, const FFind *f, const unsigned char *selected, const char *repl,
                 time_t now, size_t *files, size_t *count, char *err, size_t esz)
{
    size_t rl = strlen(repl), done_files = 0, done = 0, i = 0;

    if (memchr(repl, '\n', rl) || memchr(repl, '\r', rl)) {
        snprintf(err, esz, "La substitució no pot tenir salts de línia.");
        return -1;
    }
    while (i < f->n) {
        FNode *file = f->hits[i].file;
        size_t j = i, k, len, o = 0, pos = 0, nsel = 0;
        char *text, *res, serr[200];
        struct stat st;

        while (j < f->n && f->hits[j].file == file)
            j++;
        for (k = i; k < j; k++)
            nsel += !selected || selected[k];
        if (!nsel) {
            i = j;
            continue;
        }
        if (stat(file->path, &st) != 0 || (long long)st.st_size != (long long)file->fsize ||
            st.st_ino != file->ino ||
            ((long long)st.st_mtim.tv_sec * 1000000000LL + st.st_mtim.tv_nsec) != file->mtime) {
            snprintf(err, esz, "«%s» ha canviat des de la cerca; torna a cercar. No s'ha tocat.", file->title);
            if (files)
                *files = done_files;
            if (count)
                *count = done;
            return -1;
        }
        text = read_whole(file->path, &len);
        if (!text) {
            snprintf(err, esz, "No s'ha pogut llegir «%s»: %s", file->title, strerror(errno));
            return -1;
        }
        if (project_snapshot(p, file, 1, now, serr, sizeof serr) < 0) {
            snprintf(err, esz, "Sense instantània de «%s» (%s); no s'ha tocat.", file->title, serr);
            free(text);
            return -1;
        }
        res = xmalloc(len + nsel * (rl + 1) + 8);
        for (k = i; k < j; k++) {
            if (selected && !selected[k])
                continue;
            memcpy(res + o, text + pos, f->hits[k].start - pos);
            o += f->hits[k].start - pos;
            memcpy(res + o, repl, rl);
            o += rl;
            pos = f->hits[k].end;
        }
        memcpy(res + o, text + pos, len - pos);
        o += len - pos;
        if (write_file_atomic(file->path, res, o) != 0) {
            snprintf(err, esz, "No s'ha pogut desar «%s»: %s", file->title, strerror(errno));
            free(text);
            free(res);
            return -1;
        }
        free(text);
        free(res);
        done_files++;
        done += nsel;
        i = j;
    }
    if (files)
        *files = done_files;
    if (count)
        *count = done;
    return 0;
}
