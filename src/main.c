#include "command.h"
#include "editor.h"
#include "keys.h"
#include "render.h"
#include "tui.h"
#include "utf8.h"
#include "util.h"

#include <errno.h>
#include <langinfo.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PHILEAS_VERSION  "0.1.0"
#define DEFAULT_COLUMNS  72

static void usage(FILE *f)
{
    fputs("Ús: phileas [-c COLUMNES] [FITXER]\n"
          "\n"
          "  -c N   amplada del text en columnes (per defecte 72)\n"
          "  -h     mostra aquesta ajuda\n"
          "  -v     mostra la versió\n", f);
}

int main(int argc, char **argv)
{
    Editor e;
    int columns = DEFAULT_COLUMNS;
    int opt;

    setlocale(LC_ALL, "");
    while ((opt = getopt(argc, argv, "c:hv")) != -1) {
        switch (opt) {
        case 'c':
            columns = atoi(optarg);
            if (columns < 10 || columns > 1000) {
                fputs("phileas: les columnes han d'estar entre 10 i 1000\n", stderr);
                return 2;
            }
            break;
        case 'h':
            usage(stdout);
            return 0;
        case 'v':
            puts("Phileas " PHILEAS_VERSION);
            return 0;
        default:
            usage(stderr);
            return 2;
        }
    }
    if (argc - optind > 1) {
        fputs("phileas: només es pot obrir un document alhora\n", stderr);
        return 2;
    }

    ed_init(&e, columns);
    if (optind < argc) {
        const char *path = argv[optind];

        if (buf_load(&e.buf, path) < 0) {
            if (errno != ENOENT) {
                fprintf(stderr, "phileas: %s: %s\n", path, strerror(errno));
                ed_free(&e);
                return 1;
            }
            ed_msg(&e, "Fitxer nou: %s", path);
        }
        e.filename = xstrdup(path);
    }
    if (strcmp(nl_langinfo(CODESET), "UTF-8") != 0)
        ed_msg(&e, "Avís: la configuració regional no és UTF-8; els accents no es veuran bé");

    ui_init();
    die_hook = ui_end;
    cmd_bind_defaults();

    while (!e.quit) {
        const Command *c;
        int key, confirm;

        render(&e);
        refresh();
        key = key_read();
        if (key < 0 || key == (KEY_FLAG_FN | KEY_RESIZE))
            continue;
        e.msg[0] = '\0';
        confirm = e.quit_confirm;
        if (key == KEY_PASTE) {
            size_t n;
            const char *text = key_paste_text(&n);
            if (n > 0)
                ed_insert(&e, text, n);
        } else if ((c = cmd_for_key(key))) {
            c->fn(&e);
        } else if (!(key & (KEY_FLAG_FN | KEY_FLAG_ALT)) && utf8_printable((wchar_t)key)) {
            char s[4];
            ed_insert(&e, s, utf8_encode((wchar_t)key, s));
        }
        /* L'avís de sortida només val per a la pulsació següent */
        if (e.quit_confirm == confirm)
            e.quit_confirm = 0;
    }

    ui_end();
    ed_free(&e);
    return 0;
}
