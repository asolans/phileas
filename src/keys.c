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

static int    wait_ms = -1;
static char  *paste;
static size_t paste_len, paste_cap;

/* Tecles que arriben com a caràcters normals */
static const struct {
    const char *name;
    int code;
} plain_keys[] = {
    { "enter", '\n' }, { "tab", '\t' }, { "esc", 27 },
};

/* Tecles de navegació: sense modificadors, amb el codi de ncurses; amb
 * modificadors, amb un codi propi (KEY_MODIFIED), arribin com arribin */
static const struct {
    const char *name;
    int code;          /* codi de ncurses sense modificadors */
    const char *cap;   /* arrel de les capacitats terminfo amb modificadors */
} nav_keys[] = {
    [KB_UP] = { "up", KEY_UP, "kUP" },          [KB_DOWN] = { "down", KEY_DOWN, "kDN" },
    [KB_RIGHT] = { "right", KEY_RIGHT, "kRIT" }, [KB_LEFT] = { "left", KEY_LEFT, "kLFT" },
    [KB_HOME] = { "home", KEY_HOME, "kHOM" },    [KB_END] = { "end", KEY_END, "kEND" },
    [KB_PGUP] = { "pgup", KEY_PPAGE, "kPRV" },   [KB_PGDN] = { "pgdn", KEY_NPAGE, "kNXT" },
    [KB_DELETE] = { "delete", KEY_DC, "kDC" },   [KB_INSERT] = { "insert", KEY_IC, "kIC" },
};
#define NNAV ((int)(sizeof nav_keys / sizeof nav_keys[0]))

/* Prefixos de modificador i el seu valor a les seqüències xterm (\e[1;2A) */
static const struct {
    const char *prefix;
    int mod;
} mod_prefixes[] = {
    { "ctrl-alt-shift-", 8 }, { "ctrl-shift-", 6 }, { "ctrl-alt-", 7 },
    { "alt-shift-", 4 }, { "shift-alt-", 4 }, { "shift-", 2 }, { "alt-", 3 }, { "ctrl-", 5 },
};

/* Codis que ncurses retorna per a tecles amb modificadors que el terminfo
 * coneix, traduïts al codi propi */
static struct {
    int ncurses, code;
} translations[NNAV * 7];
static int ntranslations;

static int nav_index(const char *name)
{
    for (int i = 0; i < NNAV; i++)
        if (!strcmp(name, nav_keys[i].name))
            return i;
    return -1;
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
    if ((n = nav_index(name)) >= 0)
        return KEY_FLAG_FN | nav_keys[n].code;
    if (!strcmp(name, "backspace"))
        return KEY_FLAG_FN | KEY_BACKSPACE;
    for (size_t i = 0; i < sizeof mod_prefixes / sizeof mod_prefixes[0]; i++) {
        size_t len = strlen(mod_prefixes[i].prefix);

        if (!strncmp(name, mod_prefixes[i].prefix, len) && (n = nav_index(name + len)) >= 0)
            return KEY_MODIFIED(n, mod_prefixes[i].mod);
    }

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

void key_set_timeout(int ms)
{
    wait_ms = ms;
    timeout(ms);
}

void key_init(void)
{
    ntranslations = 0;
    for (int i = 0; i < NNAV; i++) {
        for (int mod = 2; mod <= 8; mod++) {
            char cap[16];
            char *seq;
            int k;

            snprintf(cap, sizeof cap, mod == 2 ? "%s" : "%s%d", nav_keys[i].cap, mod);
            seq = tigetstr(cap);
            if (!seq || seq == (char *)-1 || (k = key_defined(seq)) <= 0)
                continue;
            translations[ntranslations].ncurses = k;
            translations[ntranslations++].code = KEY_MODIFIED(i, mod);
        }
    }
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
    timeout(-1);
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
    timeout(wait_ms);
}

const char *key_paste_text(size_t *n)
{
    *n = paste_len;
    return paste;
}

/* Tecla d'una seqüència CSI (\e[...) o SS3 (\eO...) que ncurses no ha
 * reconegut, perquè el terminfo no la té: per exemple, Maj+fletxa a la
 * consola de Linux amb un mapa de teclat que envia seqüències xterm.
 * Retorna -1 si no se sap què és (i es descarta). */
static int escape_key(int intro, const char *params, int final)
{
    static const char finals[] = "ABCDHF";
    static const int final_nav[] = { KB_UP, KB_DOWN, KB_RIGHT, KB_LEFT, KB_HOME, KB_END };
    int p1 = 0, mod = 1, nav = -1;
    const char *semi = strchr(params, ';'), *f;

    if (*params)
        p1 = atoi(params);
    if (semi)
        mod = atoi(semi + 1);
    if (final != '~' && (f = strchr(finals, final)) && final)
        nav = final_nav[f - finals];
    else if (intro == 'O' && final >= 'P' && final <= 'S')
        return mod <= 1 ? KEY_FLAG_FN | KEY_F(final - 'P' + 1) : -1;
    else if (intro == '[' && final == '~') {
        switch (p1) {
        case 1: case 7: nav = KB_HOME; break;
        case 4: case 8: nav = KB_END; break;
        case 2: nav = KB_INSERT; break;
        case 3: nav = KB_DELETE; break;
        case 5: nav = KB_PGUP; break;
        case 6: nav = KB_PGDN; break;
        default: {
            /* F1-F12: 11-15, 17-21, 23-24 */
            static const int fn[] = { 11, 12, 13, 14, 15, 17, 18, 19, 20, 21, 23, 24 };

            for (int i = 0; i < 12; i++)
                if (p1 == fn[i])
                    return mod <= 1 ? KEY_FLAG_FN | KEY_F(i + 1)
                         : mod == 2 ? KEY_FLAG_FN | KEY_F(i + 13) : -1;
            return -1;
        }
        }
    }
    if (nav < 0)
        return -1;
    if (mod <= 1)
        return KEY_FLAG_FN | nav_keys[nav].code;
    return mod <= 8 ? KEY_MODIFIED(nav, mod) : -1;
}

/* Llegeix la resta d'una seqüència d'escapament després de \e[ o \eO. */
static int read_escape(int intro)
{
    char params[32];
    size_t n = 0;
    wint_t wc;
    int r, key = -1;

    timeout(ALT_WAIT);
    while ((r = get_wch(&wc)) == OK) {
        if (wc >= 0x40 && wc <= 0x7E) {   /* byte final */
            params[n] = '\0';
            if (intro == '[' && !strcmp(params, "200") && wc == '~') {
                timeout(wait_ms);
                read_paste();
                return KEY_PASTE;
            }
            key = escape_key(intro, params, (int)wc);
            break;
        }
        if (n + 1 < sizeof params)
            params[n++] = (char)wc;
    }
    timeout(wait_ms);
    /* Sense res darrere: era Alt+[ o Alt+O */
    if (r != OK && n == 0)
        return KEY_FLAG_ALT | intro;
    return key;
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
        for (int i = 0; i < ntranslations; i++)
            if (translations[i].ncurses == (int)wc)
                return translations[i].code;
        return KEY_FLAG_FN | (int)wc;
    }
    if (wc == '\r')
        return '\n';
    if (wc == 127)
        return KEY_FLAG_FN | KEY_BACKSPACE;
    if (wc == 27) {
        timeout(ALT_WAIT);
        r = get_wch(&wc2);
        timeout(wait_ms);
        if (r == OK && (wc2 == '[' || wc2 == 'O'))
            return read_escape((int)wc2);
        if (r == OK)
            return KEY_FLAG_ALT | (int)(wc2 == '\r' ? '\n' : wc2);
        if (r == KEY_CODE_YES)
            return KEY_FLAG_FN | (int)wc2;
    }
    return (int)wc;
}
