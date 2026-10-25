#include "keys.h"
#include "tui.h"
#include "utf8.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Temps d'espera (ms) per distingir Alt+tecla d'un Esc sol */
#define ALT_WAIT 25

/* Codis propis per a l'inici i el final d'un enganxat ("bracketed paste"),
 * lluny dels que ncurses assigna a les tecles ampliades de terminfo */
#define PASTE_BEGIN (KEY_MAX + 0x200)
#define PASTE_END   (KEY_MAX + 0x201)

static char  *paste;
static size_t paste_len, paste_cap;

/* Tecles que arriben com a caràcters normals */
static const struct {
    const char *name;
    int code;
} plain_keys[] = {
    { "enter", '\n' }, { "tab", '\t' }, { "esc", 27 },
};

static const struct {
    const char *name;
    int code;
} named_keys[] = {
    { "up", KEY_UP },          { "down", KEY_DOWN },
    { "left", KEY_LEFT },      { "right", KEY_RIGHT },
    { "home", KEY_HOME },      { "end", KEY_END },
    { "pgup", KEY_PPAGE },     { "pgdn", KEY_NPAGE },
    { "insert", KEY_IC },      { "delete", KEY_DC },
    { "backspace", KEY_BACKSPACE },
    { "shift-left", KEY_SLEFT }, { "shift-right", KEY_SRIGHT },
    { "shift-home", KEY_SHOME }, { "shift-end", KEY_SEND },
    { "shift-pgup", KEY_SPREVIOUS }, { "shift-pgdn", KEY_SNEXT },
};

/* Tecles que ncurses no té com a constant: es busquen a terminfo */
static const struct {
    const char *name;
    const char *cap;
} ext_keys[] = {
    { "ctrl-left", "kLFT5" },  { "ctrl-right", "kRIT5" },
    { "ctrl-up", "kUP5" },     { "ctrl-down", "kDN5" },
    { "ctrl-home", "kHOM5" },  { "ctrl-end", "kEND5" },
    { "ctrl-delete", "kDC5" },
    { "shift-up", "kUP" },     { "shift-down", "kDN" },
    { "ctrl-shift-left", "kLFT6" }, { "ctrl-shift-right", "kRIT6" },
};

static int ext_key(const char *cap)
{
    char *s = tigetstr(cap);
    int k;

    if (!s || s == (char *)-1)
        return -1;
    k = key_defined(s);
    return k > 0 ? KEY_FLAG_FN | k : -1;
}

static int fkey_number(const char *s)
{
    char *end;
    long n = strtol(s, &end, 10);

    return (s[0] >= '1' && s[0] <= '9' && !*end && n <= 12) ? (int)n : -1;
}

int key_parse(const char *name)
{
    int n;

    for (size_t i = 0; i < sizeof plain_keys / sizeof plain_keys[0]; i++)
        if (!strcmp(name, plain_keys[i].name))
            return plain_keys[i].code;
    for (size_t i = 0; i < sizeof named_keys / sizeof named_keys[0]; i++)
        if (!strcmp(name, named_keys[i].name))
            return KEY_FLAG_FN | named_keys[i].code;
    for (size_t i = 0; i < sizeof ext_keys / sizeof ext_keys[0]; i++)
        if (!strcmp(name, ext_keys[i].name))
            return ext_key(ext_keys[i].cap);

    if (name[0] == 'f' && (n = fkey_number(name + 1)) > 0)
        return KEY_FLAG_FN | KEY_F(n);
    if (!strncmp(name, "shift-f", 7) && (n = fkey_number(name + 7)) > 0)
        return KEY_FLAG_FN | KEY_F(n + 12);

    if (!strcmp(name, "ctrl-space"))
        return 0;
    if (!strncmp(name, "ctrl-", 5) && name[5] && !name[6]) {
        int c = name[5];
        /* Ctrl+I i Ctrl+M són idèntiques a Tab i Enter */
        if (c < 'a' || c > 'z' || c == 'i' || c == 'm')
            return -1;
        return c & 0x1F;
    }
    if (!strncmp(name, "alt-", 4) && name[4]) {
        const char *s = name + 4;
        wchar_t wc;
        size_t len;

        if (!strcmp(s, "enter"))
            return KEY_FLAG_ALT | '\n';
        len = utf8_decode(s, strlen(s), 0, &wc);
        if (wc != UTF8_INVALID && !s[len])
            return KEY_FLAG_ALT | wc;
    }
    return -1;
}

void key_init(void)
{
    define_key("\033[200~", PASTE_BEGIN);
    define_key("\033[201~", PASTE_END);
    /* Activa l'enganxat delimitat: el terminal avisa a l'inici i al final
     * d'un text enganxat, que així s'insereix tal qual i d'una sola vegada */
    fputs("\033[?2004h", stdout);
    fflush(stdout);
}

void key_end(void)
{
    fputs("\033[?2004l", stdout);
    fflush(stdout);
    free(paste);
    paste = NULL;
    paste_len = paste_cap = 0;
}

static void paste_add(wchar_t wc)
{
    char buf[4];
    size_t n = utf8_encode(wc, buf);

    if (paste_len + n > paste_cap) {
        paste_cap = paste_cap ? paste_cap * 2 : 4096;
        paste = xrealloc(paste, paste_cap);
    }
    memcpy(paste + paste_len, buf, n);
    paste_len += n;
}

static void read_paste(void)
{
    wint_t wc;
    int r;

    paste_len = 0;
    while ((r = get_wch(&wc)) != ERR) {
        if (r == KEY_CODE_YES) {
            if (wc == PASTE_END)
                break;
            if (wc == KEY_ENTER)
                paste_add(L'\n');
        } else if (wc == '\r' || wc == '\n' || wc == '\t' || wc >= ' ') {
            paste_add(wc == '\r' ? L'\n' : (wchar_t)wc);
        }
    }
}

const char *key_paste_text(size_t *n)
{
    *n = paste_len;
    return paste;
}

int key_read(void)
{
    wint_t wc, wc2;
    int r = get_wch(&wc);

    if (r == ERR)
        return -1;
    if (r == KEY_CODE_YES) {
        if (wc == PASTE_BEGIN) {
            read_paste();
            return KEY_PASTE;
        }
        if (wc == KEY_ENTER)
            return '\n';
        return KEY_FLAG_FN | (int)wc;
    }
    if (wc == '\r')
        return '\n';
    if (wc == 127)
        return KEY_FLAG_FN | KEY_BACKSPACE;
    if (wc == 27) {
        timeout(ALT_WAIT);
        r = get_wch(&wc2);
        timeout(-1);
        if (r == OK)
            return KEY_FLAG_ALT | (int)(wc2 == '\r' ? '\n' : wc2);
        if (r == KEY_CODE_YES)
            return KEY_FLAG_FN | (int)wc2;
    }
    return (int)wc;
}
