#include "compile.h"
#include "util.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/wait.h>
#include <unistd.h>

static char *read_text(const char *path, size_t *len)
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

static int is_comment_line(const char *s, size_t len)
{
    size_t i = 0, e = len;

    while (i < e && (s[i] == ' ' || s[i] == '\t'))
        i++;
    while (e > i && (s[e - 1] == ' ' || s[e - 1] == '\t'))
        e--;
    return e - i >= 7 && !strncmp(s + i, "<!--", 4) && !strncmp(s + e - 3, "-->", 3);
}

/* Nivell d'un títol ATX («### x» → 3), o 0 */
static int atx_level(const char *s, size_t len, size_t *after)
{
    size_t sp = 0, lv = 0;

    while (sp < len && sp < 3 && s[sp] == ' ')
        sp++;
    while (sp + lv < len && s[sp + lv] == '#')
        lv++;
    if (lv < 1 || lv > 6 || (sp + lv < len && s[sp + lv] != ' ' && s[sp + lv] != '\t'))
        return 0;
    *after = sp + lv;
    return (int)lv;
}

static int is_fence(const char *s, size_t len)
{
    size_t sp = 0;

    while (sp < len && sp < 3 && s[sp] == ' ')
        sp++;
    return len - sp >= 3 && (!strncmp(s + sp, "```", 3) || !strncmp(s + sp, "~~~", 3));
}

/* Treu les notes %%així%% d'una línia (dins de la mateixa línia) */
static size_t strip_notes(char *line, size_t len)
{
    size_t o = 0;

    for (size_t i = 0; i < len;) {
        if (i + 1 < len && line[i] == '%' && line[i + 1] == '%') {
            size_t j = i + 2;

            while (j + 1 < len && !(line[j] == '%' && line[j + 1] == '%'))
                j++;
            if (j + 1 < len) {
                i = j + 2;
                if (o && line[o - 1] == ' ' && (i >= len || line[i] == ' '))
                    o--;                              /* no deixa un espai doble */
                continue;
            }
        }
        line[o++] = line[i++];
    }
    line[o] = '\0';
    return o;
}

/* [[Nom]] → Nom, [[Nom|text]] → text, [[Nom#secció]] → Nom */
static size_t strip_links(char *line, size_t len)
{
    size_t o = 0;

    for (size_t i = 0; i < len;) {
        if (i + 1 < len && line[i] == '[' && line[i + 1] == '[') {
            size_t j = i + 2;

            while (j + 1 < len && !(line[j] == ']' && line[j + 1] == ']'))
                j++;
            if (j + 1 < len) {
                size_t ts = i + 2, te = j, bar = (size_t)-1, hash = (size_t)-1;

                for (size_t k = ts; k < te; k++) {
                    if (line[k] == '|' && bar == (size_t)-1)
                        bar = k;
                    if (line[k] == '#' && hash == (size_t)-1 && bar == (size_t)-1)
                        hash = k;
                }
                if (bar != (size_t)-1)
                    ts = bar + 1;
                else if (hash != (size_t)-1)
                    te = hash;
                while (ts < te && isspace((unsigned char)line[ts]))
                    ts++;
                while (te > ts && isspace((unsigned char)line[te - 1]))
                    te--;
                if (te > ts) {
                    memmove(line + o, line + ts, te - ts);
                    o += te - ts;
                    i = j + 2;
                    continue;
                }
            }
        }
        line[o++] = line[i++];
    }
    line[o] = '\0';
    return o;
}

static size_t count_pending(const char *s, size_t len)
{
    size_t n = 0;

    for (size_t i = 0; i < len; i++) {
        if (i + 3 <= len && !strncmp(s + i, "[?]", 3))
            n++;
        else if (i + 4 <= len && !strncmp(s + i, "TODO", 4) &&
                 (i == 0 || !isalnum((unsigned char)s[i - 1])) &&
                 (i + 4 == len || !isalnum((unsigned char)s[i + 4])))
            n++;
    }
    return n;
}

/* Escriu un capítol; retorna 1 si ha escrit alguna cosa */
static int emit_chapter(FILE *out, const char *text, size_t len, const CompileOpts *o, CompileStats *st)
{
    size_t pos = 0;
    int fence = 0, prev_heading = 0, wrote = 0, blank_run = 0;

    if (len >= 3 && !memcmp(text, "\xEF\xBB\xBF", 3))
        pos = 3;
    /* capçalera YAML */
    if (len - pos >= 4 && !strncmp(text + pos, "---", 3) && (text[pos + 3] == '\n' || text[pos + 3] == '\r')) {
        size_t i = pos;

        while (i < len && text[i] != '\n')
            i++;
        i++;
        while (i < len) {
            size_t e = i;

            while (e < len && text[e] != '\n')
                e++;
            if ((e - i == 3 || (e - i == 4 && text[e - 1] == '\r')) &&
                (!strncmp(text + i, "---", 3) || !strncmp(text + i, "...", 3))) {
                pos = e < len ? e + 1 : e;
                break;
            }
            i = e < len ? e + 1 : e;
        }
    }
    while (pos < len) {
        size_t e = pos, ll;
        char *line;
        int lv = 0, drop = 0;
        size_t after = 0;

        while (e < len && text[e] != '\n')
            e++;
        ll = e - pos;
        if (ll && text[pos + ll - 1] == '\r')
            ll--;
        line = xmalloc(ll + 8);
        memcpy(line, text + pos, ll);
        line[ll] = '\0';
        pos = e < len ? e + 1 : e;

        if (is_fence(line, ll)) {
            fence = !fence;
        } else if (!fence) {
            if (prev_heading && is_comment_line(line, ll))
                drop = 1;                                /* metadades d'escena */
            prev_heading = 0;
            if (!drop) {
                lv = atx_level(line, ll, &after);
                if (lv) {
                    prev_heading = 1;
                    if (o->scene_sep && lv == 3) {
                        free(line);
                        line = xstrdup("* * *");
                        ll = 5;
                        lv = 0;
                    } else if (o->promote && lv > 1) {
                        memmove(line + after - 1, line + after, ll - after + 1);   /* treu un # */
                        ll--;
                    }
                }
                if (!o->keep_notes)
                    ll = strip_notes(line, ll);
                if (!o->keep_links)
                    ll = strip_links(line, ll);
            }
        }
        if (drop) {
            free(line);
            continue;
        }
        if (ll == 0 && !fence) {
            blank_run++;
            free(line);
            continue;
        }
        if (blank_run && wrote)
            fputc('\n', out);
        blank_run = 0;
        fwrite(line, 1, ll, out);
        fputc('\n', out);
        if (!fence)
            st->pending += count_pending(line, ll);
        wrote = 1;
        free(line);
    }
    return wrote;
}

int project_compile(const Project *p, const CompileOpts *o, FILE *out, CompileStats *st,
                    char *err, size_t esz)
{
    memset(st, 0, sizeof *st);
    if (!p->manuscrit || !p->manuscrit->nkids) {
        snprintf(err, esz, "El manuscrit és buit: no hi ha cap capítol per compilar.");
        return -1;
    }
    fputs("---\n", out);
    {
        /* camps entre cometes dobles per no equivocar-se amb els dos punts */
        const char *kv[][2] = { { "title", p->titol }, { "author", p->autor }, { "lang", p->idioma } };

        for (int i = 0; i < 3; i++) {
            const char *v = kv[i][1];

            if (!v || !*v)
                continue;
            fprintf(out, "%s: \"", kv[i][0]);
            for (; *v; v++) {
                if (*v == '"' || *v == '\\')
                    fputc('\\', out);
                fputc((*v == '\n' || *v == '\r') ? ' ' : *v, out);
            }
            fputs("\"\n", out);
        }
    }
    fputs("---\n", out);
    for (size_t i = 0; i < p->manuscrit->nkids; i++) {
        const FNode *c = p->manuscrit->kids[i];
        size_t len;
        char *text = read_text(c->path, &len);

        if (!text) {
            snprintf(err, esz, "No s'ha pogut llegir «%s»: %s", c->title, strerror(errno));
            return -1;
        }
        fputc('\n', out);
        if (emit_chapter(out, text, len, o, st))
            st->chapters++;
        st->words += c->words;
        free(text);
    }
    if (ferror(out) || fflush(out) != 0) {
        snprintf(err, esz, "No s'ha pogut escriure la sortida: %s", strerror(errno));
        return -1;
    }
    return 0;
}

static int known_format(const char *out)
{
    static const char *const ext[] = { ".docx", ".odt", ".epub", ".pdf", ".html", ".htm", ".rtf", ".tex", ".txt", ".rst" };
    const char *dot = strrchr(out, '.');

    for (size_t i = 0; dot && i < sizeof ext / sizeof *ext; i++)
        if (!strcasecmp(dot, ext[i]))
            return 1;
    return 0;
}

int compile_needs_pandoc(const char *out)
{
    const char *dot = strrchr(out, '.');

    return dot && strcasecmp(dot, ".md") && strcasecmp(dot, ".markdown");
}

int compile_pandoc(const char *md, const char *out, char *err, size_t esz)
{
    pid_t pid;
    int status = 0, errpipe[2];
    char msg[300] = "";

    if (pipe(errpipe) != 0) {
        snprintf(err, esz, "No s'ha pogut executar pandoc: %s", strerror(errno));
        return -1;
    }
    pid = fork();
    if (pid == 0) {
        int dn = open("/dev/null", O_RDWR);

        dup2(dn, 0);
        dup2(dn, 1);
        dup2(errpipe[1], 2);
        close(errpipe[0]);
        close(errpipe[1]);
        execlp("pandoc", "pandoc", "-f", "markdown", md, "-o", out, "--standalone", (char *)NULL);
        _exit(127);
    }
    close(errpipe[1]);
    if (pid > 0) {
        ssize_t n = read(errpipe[0], msg, sizeof msg - 1);

        if (n > 0)
            msg[n] = '\0';
        while (waitpid(pid, &status, 0) < 0 && errno == EINTR)
            ;
    }
    close(errpipe[0]);
    if (pid < 0) {
        snprintf(err, esz, "No s'ha pogut crear el procés de pandoc.");
        return -1;
    }
    if (WIFEXITED(status) && WEXITSTATUS(status) == 127) {
        snprintf(err, esz, "Cal pandoc per generar aquest format (no és instal·lat); el .md sí que es pot fer.");
        return -1;
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        for (char *s = msg; *s; s++)
            if (*s == '\n')
                *s = ' ';
        snprintf(err, esz, "pandoc ha fallat: %.200s", msg);
        return -1;
    }
    return 0;
}

/* El fitxer de sortida no pot trepitjar res del projecte: ni projecte.md ni
 * res de dins de les carpetes de capítols, fitxes, plantilles, .versions o .paperera */
static int unsafe_output(const Project *p, const char *outpath)
{
    char *copy = xstrdup(outpath), *slash = strrchr(copy, '/'), *dir, *base;
    char *real;
    int bad = 0;

    if (slash) {
        *slash = '\0';
        dir = copy[0] ? copy : (char *)"/";
        base = slash + 1;
    } else {
        dir = (char *)".";
        base = copy;
    }
    real = realpath(dir, NULL);
    if (real) {
        size_t rl = strlen(p->root);

        if (!strcmp(real, p->root) && !strcmp(base, "projecte.md"))
            bad = 1;
        else if (!strncmp(real, p->root, rl) && real[rl] == '/') {
            const char *sub = real + rl + 1;
            size_t cl = strcspn(sub, "/");

            if (cl) {
                for (size_t g = 0; g < p->top->nkids && !bad; g++)
                    bad = strlen(p->top->kids[g]->name) == cl && !strncmp(p->top->kids[g]->name, sub, cl);
                static const char *const reserved[] = { ".versions", ".paperera", "plantilles" };

                for (size_t r = 0; r < 3; r++)
                    if (strlen(reserved[r]) == cl && !strncmp(sub, reserved[r], cl))
                        bad = 1;
            }
        }
        free(real);
    }
    free(copy);
    return bad;
}

int project_compile_file(const Project *p, const CompileOpts *o, const char *outpath, CompileStats *st,
                         char *err, size_t esz)
{
    size_t pl = strlen(outpath);
    char *tmp;
    FILE *f;
    int needs = compile_needs_pandoc(outpath), rc = -1, fd;

    if (unsafe_output(p, outpath)) {
        snprintf(err, esz, "No es pot compilar dins d'una carpeta del projecte ni sobre projecte.md.");
        return -1;
    }
    if (needs && !known_format(outpath)) {
        snprintf(err, esz, "Format desconegut: faig .md, .docx, .odt, .epub, .pdf, .html, .rtf, .tex, .txt o .rst.");
        return -1;
    }
    tmp = xmalloc(pl + 48);
    snprintf(tmp, pl + 48, "%s.fabula-tmp.%ld%s", outpath, (long)getpid(), needs ? ".md" : "");
    fd = open(tmp, O_WRONLY | O_CREAT | O_EXCL, 0666);
    f = fd >= 0 ? fdopen(fd, "wb") : NULL;
    if (!f) {
        if (fd >= 0)
            close(fd);
        snprintf(err, esz, "No s'ha pogut crear %s: %s", outpath, strerror(errno));
        free(tmp);
        return -1;
    }
    if (project_compile(p, o, f, st, err, esz) != 0) {
        fclose(f);
        unlink(tmp);
        free(tmp);
        return -1;
    }
    if (fsync(fileno(f)) != 0 || fclose(f) != 0) {
        snprintf(err, esz, "No s'ha pogut desar %s: %s", outpath, strerror(errno));
        unlink(tmp);
        free(tmp);
        return -1;
    }
    if (needs) {
        char *out2 = xmalloc(pl + 48);

        snprintf(out2, pl + 48, "%s.fabula-tmp.%ld.%s", outpath, (long)getpid(), strrchr(outpath, '.') + 1);
        rc = compile_pandoc(tmp, out2, err, esz);
        unlink(tmp);
        if (rc == 0 && rename(out2, outpath) != 0) {
            snprintf(err, esz, "No s'ha pogut desar %s: %s", outpath, strerror(errno));
            rc = -1;
        }
        if (rc != 0)
            unlink(out2);
        free(out2);
    } else {
        rc = rename(tmp, outpath);
        if (rc != 0) {
            snprintf(err, esz, "No s'ha pogut desar %s: %s", outpath, strerror(errno));
            unlink(tmp);
        }
    }
    free(tmp);
    return rc;
}
