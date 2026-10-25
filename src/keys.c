#include "keys.h"
#include "tui.h"
#include "utf8.h"

#include <stdlib.h>
#include <string.h>

/* Temps d'espera (ms) per distingir Alt+tecla d'un Esc sol */
#define ALT_WAIT 25

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

int key_read(void)
{
    wint_t wc, wc2;
    int r = get_wch(&wc);

    if (r == ERR)
        return -1;
    if (r == KEY_CODE_YES)
        return KEY_FLAG_FN | (int)wc;
    if (wc == 27) {
        timeout(ALT_WAIT);
        r = get_wch(&wc2);
        timeout(-1);
        if (r == OK)
            return KEY_FLAG_ALT | (int)wc2;
        if (r == KEY_CODE_YES)
            return KEY_FLAG_FN | (int)wc2;
    }
    return (int)wc;
}
