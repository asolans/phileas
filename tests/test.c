/* Proves dels mòduls que no depenen de ncurses: make test */
#include "buffer.h"
#include "md.h"
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

    /* Sagnat penjant: les files de continuació s'alineen amb el text */
    l = make_line("- un element de llista llarg");
    wrap_line(&l, 12);
    CHECK(l.hang == 2);
    CHECK(l.nrows == 3 && l.rows[1] == 13 && l.rows[2] == 23);
    CHECK(wrap_row_indent(&l, 0) == 0 && wrap_row_indent(&l, 1) == 2);
    free(l.rows);

    l = make_line("> cita llarga que fa wrap");
    wrap_line(&l, 10);
    CHECK(l.hang == 2 && l.nrows == 4);
    free(l.rows);

    /* Prefix més ample que mitja columna: sense sagnat */
    l = make_line("12345678. massa ample");
    wrap_line(&l, 12);
    CHECK(l.hang == 0);
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

/* Contingut del buffer com a text amb '\n' */
static char *dump(Buffer *b)
{
    size_t n;
    Line *last = &b->lines[b->nlines - 1];

    return buf_extract(b, (Pos){ 0, 0 }, (Pos){ b->nlines - 1, last->len }, &n);
}

static int dump_is(Buffer *b, const char *expect)
{
    char *s = dump(b);
    int ok = strcmp(s, expect) == 0;

    if (!ok)
        fprintf(stderr, "  buffer: \"%s\"\n  esperat: \"%s\"\n", s, expect);
    free(s);
    return ok;
}

static void test_edit(void)
{
    Buffer b;
    Pos p;
    char *s;
    size_t n;

    buf_init(&b);
    p = buf_insert(&b, (Pos){ 0, 0 }, "hola", 4);
    CHECK(p.line == 0 && p.col == 4 && b.modified);
    p = buf_insert(&b, (Pos){ 0, 2 }, "XY", 2);
    CHECK(dump_is(&b, "hoXYla") && p.col == 4);

    /* Inserció amb salts: el que hi havia darrere passa a l'última línia */
    p = buf_insert(&b, (Pos){ 0, 2 }, "1\n2\n3", 5);
    CHECK(dump_is(&b, "ho1\n2\n3XYla") && b.nlines == 3);
    CHECK(p.line == 2 && p.col == 1);
    p = buf_insert(&b, (Pos){ 2, 5 }, "\n", 1);
    CHECK(dump_is(&b, "ho1\n2\n3XYla\n") && p.line == 3 && p.col == 0);

    s = buf_extract(&b, (Pos){ 0, 1 }, (Pos){ 2, 2 }, &n);
    CHECK(n == 7 && strcmp(s, "o1\n2\n3X") == 0);
    free(s);

    /* Esborrar dins una línia i entre línies */
    buf_delete(&b, (Pos){ 2, 1 }, (Pos){ 2, 3 });
    CHECK(dump_is(&b, "ho1\n2\n3la\n"));
    buf_delete(&b, (Pos){ 0, 2 }, (Pos){ 2, 1 });
    CHECK(dump_is(&b, "hola\n") && b.nlines == 2);
    buf_delete(&b, (Pos){ 0, 4 }, (Pos){ 1, 0 });
    CHECK(dump_is(&b, "hola") && b.nlines == 1);

    /* Les caches s'invaliden en editar */
    wrap_line(&b.lines[0], 10);
    CHECK(b.lines[0].wrap_w == 10);
    buf_insert(&b, (Pos){ 0, 4 }, " i adéu", 8);
    CHECK(b.lines[0].wrap_w == 0 && b.lines[0].print_lines == 0);

    /* Moltes línies de cop (fa créixer el vector) */
    {
        char big[2000];
        for (int i = 0; i < 1000; i++) {
            big[2 * i] = 'a';
            big[2 * i + 1] = '\n';
        }
        p = buf_insert(&b, (Pos){ 0, 0 }, big, sizeof big);
        CHECK(b.nlines == 1001 && p.line == 1000 && p.col == 0);
        buf_delete(&b, (Pos){ 0, 0 }, p);
        CHECK(dump_is(&b, "hola i adéu") && b.nlines == 1);
    }
    buf_free(&b);
}

static void test_md(void)
{
    char out[64];
    const char *s;

#define PREFIX(str) md_prefix(str, strlen(str))
#define CONT(str) (md_continue(str, strlen(str), out, sizeof out), out)
    CHECK(PREFIX("text normal") == 0);
    CHECK(PREFIX("- element") == 2);
    CHECK(PREFIX("*èmfasi*") == 0);
    CHECK(PREFIX("-guió") == 0);
    CHECK(PREFIX("  * niat") == 4);
    CHECK(PREFIX("12. dotze") == 4);
    CHECK(PREFIX("3) tres") == 3);
    CHECK(PREFIX("> cita") == 2);
    CHECK(PREFIX("> > - dins") == 6);
    CHECK(PREFIX("- [ ] tasca") == 6);
    CHECK(PREFIX("- [x] feta") == 6);
    CHECK(PREFIX("    sagnat") == 4);
    CHECK(PREFIX("2024. any") == 6);        /* sembla una llista: és Markdown estàndard */
    CHECK(md_marker("    sagnat", 10) == 0);
    CHECK(md_marker("- ", 2) == 2);

    CHECK(strcmp(CONT("- a"), "- ") == 0);
    CHECK(strcmp(CONT("  * b"), "  * ") == 0);
    CHECK(strcmp(CONT("9. nou"), "10. ") == 0);
    CHECK(strcmp(CONT("1) u"), "2) ") == 0);
    CHECK(strcmp(CONT("> cita"), "> ") == 0);
    CHECK(strcmp(CONT("> 1. x"), "> 2. ") == 0);
    CHECK(strcmp(CONT("- [x] feta"), "- [ ] ") == 0);
    CHECK(strcmp(CONT("    codi"), "    ") == 0);
    CHECK(strcmp(CONT("text"), "") == 0);
    s = "- a";
    CHECK(md_continue(s, 3, out, 2) == 1 && strcmp(out, "-") == 0);
#undef PREFIX
#undef CONT
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
    test_edit();
    test_md();
    rmdir(tmpdir);

    printf("%d comprovacions, %d errors\n", checks, failures);
    return failures != 0;
}
