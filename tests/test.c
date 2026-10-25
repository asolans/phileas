/* Proves dels mòduls que no depenen de ncurses: make test */
#include "buffer.h"
#include "utf8.h"
#include "wrap.h"

#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

static char tmpdir[] = "/tmp/phileas-test-XXXXXX";

static void test_utf8(void)
{
    wchar_t wc;

    CHECK(utf8_decode("à", 2, 0, &wc) == 2 && wc == 0xE0);
    CHECK(utf8_decode("€", 3, 0, &wc) == 3 && wc == 0x20AC);
    CHECK(utf8_decode("\xFF", 1, 0, &wc) == 1 && wc == UTF8_INVALID);
    CHECK(utf8_decode("\xC0\x80", 2, 0, &wc) == 1 && wc == UTF8_INVALID); /* sobrellarg */
    CHECK(utf8_decode("\xC3", 1, 0, &wc) == 1 && wc == UTF8_INVALID);     /* truncat */

    CHECK(utf8_prev("aà", 3) == 1);
    CHECK(utf8_prev("a\xFF", 2) == 1);
    CHECK(utf8_next("l·l", 4, 1) == 3);
    /* e + accent combinat (U+0301) és una sola unitat de cursor */
    CHECK(utf8_next_cluster("e\xCC\x81x", 4, 0) == 3);
    CHECK(utf8_prev_cluster("e\xCC\x81x", 3) == 0);
    CHECK(utf8_count("pàgina", 7) == 6);

    for (wchar_t c = 0x20; c < 0x2000; c += 7) {
        char out[4];
        size_t n = utf8_encode(c, out);
        if (c >= 0x7F && c < 0xA0)
            continue;
        CHECK(utf8_decode(out, n, 0, &wc) == n && wc == c);
    }

    CHECK(utf8_width(L'\t', 0) == TAB_WIDTH);
    CHECK(utf8_width(L'\t', 1) == TAB_WIDTH - 1);
    CHECK(utf8_width(L'a', 0) == 1);
    CHECK(utf8_width(0x4E2D, 0) == 2);   /* 中 */
    CHECK(utf8_width(0x01, 0) == 1 && !utf8_printable(0x01));
}

static Line make_line(const char *s)
{
    Line l;

    memset(&l, 0, sizeof l);
    l.text = (char *)s;
    l.len = strlen(s);
    return l;
}

static void test_wrap(void)
{
    Line l;

    /* "hola món " ocupa 9 columnes i 10 bytes */
    l = make_line("hola món que tal");
    wrap_line(&l, 9);
    CHECK(l.nrows == 2 && l.rows[1] == 10);
    CHECK(wrap_row_of(&l, 9) == 0 && wrap_row_of(&l, 10) == 1);
    CHECK(wrap_row_end(&l, 0) == 10 && wrap_row_end(&l, 1) == l.len);
    free(l.rows);

    /* Paraula més llarga que l'amplada: es talla per caràcter */
    l = make_line("abcdefghij");
    wrap_line(&l, 4);
    CHECK(l.nrows == 3 && l.rows[1] == 4 && l.rows[2] == 8);
    free(l.rows);

    /* Els espais que no hi caben queden penjant */
    l = make_line("abcd   efgh");
    wrap_line(&l, 4);
    CHECK(l.nrows == 2 && l.rows[1] == 7);
    free(l.rows);

    l = make_line("abcd   ");
    wrap_line(&l, 4);
    CHECK(l.nrows == 1);
    free(l.rows);

    /* Línia buida: una fila */
    l = make_line("");
    wrap_line(&l, 10);
    CHECK(l.nrows == 1 && l.rows[0] == 0);

    /* La cache es recalcula en canviar l'amplada */
    l.wrap_w = 0;
    free(l.rows);
    l = make_line("una dues tres quatre");
    wrap_line(&l, 20);
    CHECK(l.nrows == 1);
    wrap_line(&l, 9);
    CHECK(l.nrows == 3 && l.rows[1] == 9 && l.rows[2] == 14);
    free(l.rows);

    /* Caràcters amples: 中文 = 4 columnes */
    l = make_line("中文中文");
    wrap_line(&l, 5);
    CHECK(l.nrows == 2 && l.rows[1] == 6);
    free(l.rows);
}

static void write_file(const char *path, const char *data, size_t n)
{
    FILE *f = fopen(path, "wb");
    fwrite(data, 1, n, f);
    fclose(f);
}

static int file_equals(const char *path, const char *data, size_t n)
{
    char buf[256];
    FILE *f = fopen(path, "rb");
    size_t r = fread(buf, 1, sizeof buf, f);

    fclose(f);
    return r == n && memcmp(buf, data, n) == 0;
}

/* Carrega i desa: el fitxer ha de quedar idèntic byte a byte. */
static void roundtrip(const char *data, size_t nlines)
{
    char in[64], out[64];
    Buffer b;
    size_t n = strlen(data);

    snprintf(in, sizeof in, "%s/in", tmpdir);
    snprintf(out, sizeof out, "%s/out", tmpdir);
    write_file(in, data, n);
    buf_init(&b);
    CHECK(buf_load(&b, in) == 0);
    CHECK(b.nlines == nlines);
    CHECK(buf_save(&b, out) == 0);
    CHECK(file_equals(out, data, n));
    buf_free(&b);
    unlink(in);
    unlink(out);
}

static void test_buffer(void)
{
    char path[64];
    Buffer b;

    roundtrip("", 1);
    roundtrip("\n", 1);
    roundtrip("hola\n", 1);
    roundtrip("hola", 1);
    roundtrip("# Títol\n\nUn paràgraf llarg.\n", 3);
    roundtrip("a\r\nb\r\n", 2);
    roundtrip("a\r\nb\n", 2);             /* salts barrejats: es conserva el \r */
    roundtrip("\xEF\xBB\xBF" "amb BOM\n", 1);
    roundtrip("bytes \xFF\xFE invàlids\n", 1);

    buf_init(&b);
    snprintf(path, sizeof path, "%s/crlf", tmpdir);
    write_file(path, "x\r\ny\r\n", 6);
    CHECK(buf_load(&b, path) == 0 && b.crlf && b.lines[0].len == 1);
    unlink(path);

    /* Errors: el buffer no canvia */
    CHECK(buf_load(&b, "/no/existeix") == -1);
    CHECK(buf_load(&b, tmpdir) == -1);   /* directori */
    CHECK(b.nlines == 2);

    /* Un enllaç simbòlic es manté: es desa a través de l'enllaç */
    snprintf(path, sizeof path, "%s/real", tmpdir);
    write_file(path, "vell\n", 5);
    {
        char link[64];
        snprintf(link, sizeof link, "%s/enllac", tmpdir);
        CHECK(symlink("real", link) == 0);
        CHECK(buf_save(&b, link) == 0);
        CHECK(file_equals(path, "x\r\ny\r\n", 6));
        unlink(link);
    }
    unlink(path);
    buf_free(&b);
}

static void set_text(Buffer *b, size_t n, const char *fill, size_t chars)
{
    Line *l;

    buf_free(b);
    b->lines = malloc(n * sizeof *b->lines);
    b->cap = b->nlines = n;
    for (size_t i = 0; i < n; i++) {
        l = &b->lines[i];
        memset(l, 0, sizeof *l);
        l->text = malloc(chars * strlen(fill) + 1);
        l->text[0] = '\0';
        for (size_t k = 0; k < chars; k++)
            strcat(l->text, fill);
        l->len = strlen(l->text);
        l->cap = l->len + 1;
    }
}

static void test_pages(void)
{
    Buffer b;

    buf_init(&b);
    CHECK(buf_pages(&b) == 1);                 /* document buit */

    set_text(&b, 46, "a", 80);                 /* 46 línies plenes */
    CHECK(buf_pages(&b) == 1);
    set_text(&b, 47, "a", 80);
    CHECK(buf_pages(&b) == 2);
    set_text(&b, 23, "à", 160);                /* es compten caràcters, no bytes */
    CHECK(buf_pages(&b) == 1);
    set_text(&b, 23, "à", 161);
    CHECK(buf_pages(&b) == 2);

    /* Paràgrafs separats per línies en blanc (mitja línia cadascuna):
     * 31 paràgrafs d'una línia + 30 separacions = 46 línies */
    set_text(&b, 61, "a", 80);
    for (size_t i = 1; i < 61; i += 2)
        b.lines[i].len = 0;
    CHECK(buf_pages(&b) == 1);
    b.lines[1].len = 80;                       /* una separació passa a ser text */
    b.lines[1].print_lines = 0;
    CHECK(buf_pages(&b) == 2);

    set_text(&b, 46, "a", 80);
    b.lines[45].len = 0;                        /* línia en blanc final */
    b.lines[45].print_lines = 0;
    b.lines[0].text[0] = '#';                   /* capçalera: +1 línia */
    b.lines[0].print_lines = 0;
    CHECK(buf_pages(&b) == 1);
    buf_free(&b);
}

int main(void)
{
    if (!setlocale(LC_ALL, "C.UTF-8")) {
        fputs("cal la configuració regional C.UTF-8\n", stderr);
        return 1;
    }
    if (!mkdtemp(tmpdir)) {
        perror("mkdtemp");
        return 1;
    }
    test_utf8();
    test_wrap();
    test_buffer();
    test_pages();
    rmdir(tmpdir);

    printf("%d comprovacions, %d errors\n", checks, failures);
    return failures != 0;
}
