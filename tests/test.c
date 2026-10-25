/* Proves dels mòduls que no depenen de ncurses: make test */
#include "buffer.h"
#include "md.h"
#include "search.h"
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
    if (b.nlines != nlines || !file_equals(out, data, n))
        fprintf(stderr, "  fitxer: \"%s\" (%zu línies)\n", data, b.nlines);
    buf_free(&b);
    unlink(in);
    unlink(out);
}

/* Carrega data i comprova el nombre de línies i el que es desa. */
static void normalized(const char *data, size_t nlines, const char *expect)
{
    char in[64], out[64];
    Buffer b;

    snprintf(in, sizeof in, "%s/in", tmpdir);
    snprintf(out, sizeof out, "%s/out", tmpdir);
    write_file(in, data, strlen(data));
    buf_init(&b);
    CHECK(buf_load(&b, in) == 0);
    CHECK(b.nlines == nlines);
    CHECK(buf_save(&b, out) == 0);
    CHECK(file_equals(out, expect, strlen(expect)));
    if (!file_equals(out, expect, strlen(expect)))
        fprintf(stderr, "  entrada: \"%s\"\n", data);
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
    roundtrip("# Títol\n\nUn paràgraf llarg.\n", 2);
    roundtrip("a\r\n\r\nb\r\n", 2);
    roundtrip("a\r\n\nb\n", 2);           /* salts barrejats: es conserva el \r */
    roundtrip("\xEF\xBB\xBF" "amb BOM\n", 1);
    roundtrip("bytes \xFF\xFE invàlids\n", 1);

    /* Model de paràgrafs: aquests fitxers també queden idèntics */
    roundtrip("u\n\ndos\n\ntres\n", 3);
    roundtrip("- a\n- b\n  - niat\n\nText\n", 4);
    roundtrip("1. u\n2. dos\n", 2);
    roundtrip("> cita\n>\n> segona\n", 3);
    roundtrip("| a | b |\n|---|---|\n| 1 | 2 |\n", 3);
    roundtrip("```\ncodi\n\n\nmés codi\n```\n\nText\n", 7);
    roundtrip("~~~~\n```\n~~~~\n", 3);          /* tanca de la mateixa mena */
    roundtrip("---\ntitol: x\n\nautor: y\n---\n\nText\n", 6);
    roundtrip("línia amb salt  \nsegona\n", 2);
    roundtrip("barra\\\nsegona\n", 2);
    roundtrip("Títol\n=====\n\nText\n", 3);
    roundtrip("    codi sagnat\n    més\n", 2);
    roundtrip("<div>\n<p>html</p>\n</div>\n", 3);
    roundtrip("a\n\n\nb\n", 3);                  /* dues línies en blanc: una línia buida */
    roundtrip("a\n\n\n\nb\n", 4);
    roundtrip("\n\na\n", 3);                      /* en blanc al principi */
    roundtrip("```\nsense tancar\n\nfinal\n", 4);
    roundtrip("| a |\n\n| b |\n", 3);              /* dues taules separades */
    roundtrip("- a\n\n- b\n", 3);
    roundtrip("- a\n\n\nText\n", 3);
    roundtrip("- u\n    - niat\n        - més\n", 3);  /* llistes niades a 4 espais */
    roundtrip("- u\n\n    paràgraf de l'element\n", 3);
    roundtrip("- u\n    continuació sagnada\n", 2);

    /* Normalitzacions: el significat Markdown es manté */
    normalized("línia u\nlínia dos\n", 1, "línia u línia dos\n");
    normalized("- element\n  que continua\n", 1, "- element que continua\n");
    normalized("> cita\ncontinua\n", 1, "> cita continua\n");
    normalized("# Títol\nText\n", 2, "# Títol\n\nText\n");
    normalized("Intro:\n- a\n- b\n", 3, "Intro:\n\n- a\n- b\n");
    normalized("a\n\n\n", 1, "a\n");
    normalized("a\n   \nb\n", 2, "a\n\nb\n");       /* línia només amb espais */

    /* Línies buides afegides al final del buffer: no es desen */
    buf_init(&b);
    buf_insert(&b, (Pos){ 0, 0 }, "a\n\n", 3);
    snprintf(path, sizeof path, "%s/final", tmpdir);
    CHECK(buf_save(&b, path) == 0 && file_equals(path, "a\n", 2));
    unlink(path);
    buf_free(&b);

    buf_init(&b);
    snprintf(path, sizeof path, "%s/crlf", tmpdir);
    write_file(path, "x\r\n\r\ny\r\n", 8);
    CHECK(buf_load(&b, path) == 0 && b.crlf && b.nlines == 2 && b.lines[0].len == 1);
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
        CHECK(file_equals(path, "x\r\n\r\ny\r\n", 8));
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
    buf_rescan_all(b);
}

static void test_pages(void)
{
    Buffer b;

    buf_init(&b);
    CHECK(buf_pages(&b) == 1);                 /* document buit */

    set_text(&b, 1, "a", 80 * 46);             /* un paràgraf de 46 línies */
    CHECK(buf_pages(&b) == 1);
    set_text(&b, 1, "a", 80 * 46 + 1);
    CHECK(buf_pages(&b) == 2);
    set_text(&b, 1, "à", 80 * 46);             /* es compten caràcters, no bytes */
    CHECK(buf_pages(&b) == 1);

    /* Paràgrafs d'una línia separats (mitja línia cada separació):
     * 31 paràgrafs + 30 separacions = 46 línies */
    set_text(&b, 31, "a", 80);
    CHECK(buf_pages(&b) == 1);
    set_text(&b, 32, "a", 80);
    CHECK(buf_pages(&b) == 2);

    /* Els elements d'una llista van units: 46 línies sense separació */
    set_text(&b, 46, "a", 80);
    for (size_t i = 0; i < 46; i++)
        memcpy(b.lines[i].text, "- ", 2);
    buf_rescan_all(&b);
    CHECK(buf_pages(&b) == 1);

    /* Capçalera: una línia de més */
    set_text(&b, 1, "a", 80 * 45);
    b.lines[0].text[0] = '#';
    b.lines[0].text[1] = ' ';
    CHECK(buf_pages(&b) == 1);
    set_text(&b, 1, "a", 80 * 45 + 1);
    b.lines[0].text[0] = '#';
    b.lines[0].text[1] = ' ';
    CHECK(buf_pages(&b) == 2);
    buf_free(&b);
}

/* Classifica un text de diverses línies i retorna els tipus base. */
static size_t kinds(const char *text, int *out, size_t max)
{
    MdState st;
    size_t n = 0;

    memset(&st, 0, sizeof st);
    while (n < max) {
        const char *nl = strchr(text, '\n');
        size_t len = nl ? (size_t)(nl - text) : strlen(text);
        out[n++] = md_kind(text, len, &st);
        if (!nl)
            break;
        text = nl + 1;
    }
    return n;
}

static int paste_is(Buffer *b, const char *in, const char *expect)
{
    size_t n;
    char *out = buf_paste_text(b, 0, in, strlen(in), &n);
    int ok = n == strlen(expect) && !memcmp(out, expect, n);

    if (!ok)
        fprintf(stderr, "  enganxat: \"%s\" -> \"%s\"\n", in, out);
    free(out);
    return ok;
}

static void test_paste(void)
{
    Buffer b;

    buf_init(&b);
    CHECK(paste_is(&b, "a\n\nb", "a\nb"));
    CHECK(paste_is(&b, "a\nb", "a\nb"));                 /* no s'ajunta */
    CHECK(paste_is(&b, "a\n\n\nb", "a\n\nb"));
    CHECK(paste_is(&b, "a\n", "a\n"));
    CHECK(paste_is(&b, "a\n\n", "a\n"));
    CHECK(paste_is(&b, "a\n\n\n", "a\n\n"));
    CHECK(paste_is(&b, "\n\na", "\n\na"));
    CHECK(paste_is(&b, "\n", "\n"));
    CHECK(paste_is(&b, "x", "x"));
    CHECK(paste_is(&b, "```\nu\n\ndos\n```\n\nText", "```\nu\n\ndos\n```\nText"));
    CHECK(paste_is(&b, "a\n   \nb", "a\nb"));
    buf_free(&b);
}

static void test_blocks(void)
{
    int k[16];
    Buffer b;

    CHECK(kinds("text\n# Cap\n- l\n> c\n| t |\n<p>\n    codi\n===\n", k, 16) == 9);
    CHECK(k[0] == MD_TEXT && k[1] == MD_HEADING && k[2] == MD_LIST && k[3] == MD_QUOTE);
    CHECK(k[4] == MD_TABLE && k[5] == MD_HTML && k[6] == MD_INDENT && k[7] == MD_UNDERLINE);
    CHECK(k[8] == MD_EMPTY);

    CHECK(kinds("```c\nx\n\n```\ndesprés", k, 16) == 5);
    CHECK(k[0] == MD_FENCE_OPEN && k[1] == MD_CODE && k[2] == MD_CODE);
    CHECK(k[3] == MD_FENCE_CLOSE && k[4] == MD_TEXT);

    CHECK(kinds("---\na: 1\n---\n---", k, 16) == 4);
    CHECK(k[0] == MD_META && k[1] == MD_META && k[2] == MD_META && k[3] == MD_UNDERLINE);
    CHECK(kinds("text\n---", k, 16) == 2 && k[0] == MD_TEXT);   /* no són metadades */

    CHECK(kinds("#etiqueta\n####### set", k, 16) == 2);
    CHECK(k[0] == MD_TEXT && k[1] == MD_TEXT);
    CHECK(kinds("salt  \nbarra\\", k, 16) == 2);
    CHECK(k[0] == (MD_TEXT | MD_HARDBREAK) && k[1] == (MD_TEXT | MD_HARDBREAK));

    CHECK(md_tight(MD_LIST, MD_LIST) && md_tight(MD_QUOTE, MD_QUOTE));
    CHECK(!md_tight(MD_TEXT, MD_TEXT) && !md_tight(MD_TEXT, MD_LIST));
    CHECK(!md_tight(MD_LIST, MD_EMPTY) && md_tight(MD_CODE, MD_CODE));
    CHECK(kinds("    - niat\n    codi", k, 16) == 2 && k[0] == MD_LIST && k[1] == MD_INDENT);
    CHECK(md_tight(MD_TEXT | MD_HARDBREAK, MD_TEXT));
    CHECK(md_joinable(MD_TEXT, MD_TEXT) && !md_joinable(MD_HEADING, MD_TEXT));

    /* Les separacions s'actualitzen en editar */
    buf_init(&b);
    buf_insert(&b, (Pos){ 0, 0 }, "u\ndos", 6);
    CHECK(buf_gap_after(&b, 0) && !buf_gap_after(&b, 1));
    /* Enter al final: la separació es veu abans d'escriure res */
    buf_insert(&b, (Pos){ 1, 3 }, "\n", 1);
    CHECK(buf_gap_after(&b, 1) && !buf_gap_after(&b, 2));
    buf_delete(&b, (Pos){ 1, 3 }, (Pos){ 2, 0 });
    buf_insert(&b, (Pos){ 0, 0 }, "- ", 2);
    buf_insert(&b, (Pos){ 1, 0 }, "- ", 2);
    CHECK(!buf_gap_after(&b, 0));                 /* ara són una llista */
    buf_insert(&b, (Pos){ 0, 0 }, "```\n", 4);   /* obre un bloc de codi */
    CHECK(MD_BASE(b.lines[1].kind) == MD_CODE || (buf_scan(&b), MD_BASE(b.lines[1].kind) == MD_CODE));
    CHECK(!buf_gap_after(&b, 0) && !buf_gap_after(&b, 1));
    buf_delete(&b, (Pos){ 0, 0 }, (Pos){ 1, 0 }); /* el torna a treure */
    buf_scan(&b);
    CHECK(MD_BASE(b.lines[0].kind) == MD_LIST && MD_BASE(b.lines[1].kind) == MD_LIST);
    buf_free(&b);
}

static void test_undo(void)
{
    Buffer b;
    Pos c;
    char path[64];

    buf_init(&b);
    CHECK(!buf_undo(&b, &c) && !buf_redo(&b, &c));

    buf_undo_mark(&b);
    buf_insert(&b, (Pos){ 0, 0 }, "hola", 4);
    buf_insert(&b, (Pos){ 0, 4 }, " món", 5);       /* mateix grup */
    buf_undo_mark(&b);
    buf_insert(&b, (Pos){ 0, 9 }, "\nsegon", 6);
    buf_undo_mark(&b);
    buf_delete(&b, (Pos){ 0, 2 }, (Pos){ 1, 2 });
    CHECK(dump_is(&b, "hogon"));

    CHECK(buf_undo(&b, &c) && dump_is(&b, "hola món\nsegon"));
    CHECK(c.line == 1 && c.col == 2);
    CHECK(buf_undo(&b, &c) && dump_is(&b, "hola món") && c.line == 0 && c.col == 9);
    CHECK(buf_undo(&b, &c) && dump_is(&b, "") && c.col == 0);
    CHECK(!buf_undo(&b, &c) && !b.modified);
    CHECK(buf_redo(&b, &c) && dump_is(&b, "hola món") && c.col == 9 && b.modified);
    CHECK(buf_redo(&b, &c) && dump_is(&b, "hola món\nsegon"));

    /* Una edició nova descarta el que es podia refer */
    buf_undo_mark(&b);
    buf_insert(&b, (Pos){ 1, 5 }, "!", 1);
    CHECK(!buf_redo(&b, &c) && dump_is(&b, "hola món\nsegon!"));

    /* El document desat torna a estar «sense canvis» si s'hi torna desfent */
    snprintf(path, sizeof path, "%s/undo", tmpdir);
    CHECK(buf_save(&b, path) == 0 && !b.modified);
    buf_undo_mark(&b);
    buf_insert(&b, (Pos){ 0, 0 }, "X", 1);
    CHECK(b.modified);
    CHECK(buf_undo(&b, &c) && !b.modified);
    CHECK(buf_undo(&b, &c) && b.modified);
    CHECK(buf_redo(&b, &c) && !b.modified);
    unlink(path);
    buf_free(&b);
}

static void test_search(void)
{
    Buffer b;
    Pos a, z;
    int w;

    buf_init(&b);
    buf_insert(&b, (Pos){ 0, 0 }, "La Casa i la casa\nUna altra CASA\nfinal", 39);

    /* Sense majúscules al patró: no distingeix majúscules */
    CHECK(search_find(&b, (Pos){ 0, 0 }, "casa", 1, &a, &z, &w) && a.col == 3 && z.col == 7 && !w);
    CHECK(search_find(&b, (Pos){ 0, 4 }, "casa", 1, &a, &z, &w) && a.col == 13);
    CHECK(search_find(&b, (Pos){ 0, 14 }, "casa", 1, &a, &z, &w) && a.line == 1 && a.col == 10);
    CHECK(search_find(&b, (Pos){ 1, 11 }, "casa", 1, &a, &z, &w) && a.line == 0 && a.col == 3 && w);
    /* Amb majúscules: les distingeix */
    CHECK(search_find(&b, (Pos){ 0, 4 }, "Casa", 1, &a, &z, &w) && a.line == 0 && a.col == 3 && w);
    CHECK(!search_find(&b, (Pos){ 0, 0 }, "cASA", 1, &a, &z, &w));
    /* Enrere */
    CHECK(search_find(&b, (Pos){ 0, 13 }, "casa", -1, &a, &z, &w) && a.col == 3 && !w);
    CHECK(search_find(&b, (Pos){ 0, 3 }, "casa", -1, &a, &z, &w) && a.line == 1 && w);
    /* Accents i caràcters de diversos bytes */
    buf_insert(&b, (Pos){ 2, 5 }, " ÀVIA àvia", 13);
    CHECK(search_find(&b, (Pos){ 0, 0 }, "àvia", 1, &a, &z, &w) && a.line == 2 && a.col == 6);
    CHECK(z.col == 11);
    CHECK(!search_find(&b, (Pos){ 0, 0 }, "", 1, &a, &z, &w));
    CHECK(!search_find(&b, (Pos){ 0, 0 }, "inexistent", 1, &a, &z, &w));
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
    test_blocks();
    test_paste();
    test_undo();
    test_search();
    rmdir(tmpdir);

    printf("%d comprovacions, %d errors\n", checks, failures);
    return failures != 0;
}
