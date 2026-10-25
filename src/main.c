#include "autosave.h"
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
#define DEFAULT_AUTOSAVE 30     /* segons sense escriure abans de desar sol */

static void usage(FILE *f)
{
    fputs("Ús: phileas [-c COLUMNES] [-a SEGONS] [FITXER]\n"
          "\n"
          "  -c N   amplada del text en columnes (per defecte 72)\n"
          "  -a N   desa sol després de N segons sense escriure (per defecte 30;\n"
          "         0 el desactiva). Desa sobre el mateix fitxer.\n"
          "  -h     mostra aquesta ajuda\n"
          "  -v     mostra la versió\n", f);
}

int main(int argc, char **argv)
{
    Editor e;
    int columns = DEFAULT_COLUMNS, autosave = DEFAULT_AUTOSAVE;
    int opt;

    setlocale(LC_ALL, "");
    while ((opt = getopt(argc, argv, "a:c:hv")) != -1) {
        switch (opt) {
        case 'c':
            columns = atoi(optarg);
            if (columns < 10 || columns > 1000) {
                fputs("phileas: les columnes han d'estar entre 10 i 1000\n", stderr);
                return 2;
            }
            break;
        case 'a':
            autosave = atoi(optarg);
            if (autosave < 0 || autosave > 86400) {
                fputs("phileas: l'autodesat ha d'estar entre 0 i 86400 segons\n", stderr);
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
    e.autosave = autosave;
    if (optind < argc) {
        const char *path = argv[optind];

        if (ed_open(&e, path) < 0) {
            fprintf(stderr, "phileas: %s: %s\n", path, strerror(errno));
            ed_free(&e);
            return 1;
        }
    }
    if (strcmp(nl_langinfo(CODESET), "UTF-8") != 0)
        ed_msg(&e, "Avís: la configuració regional no és UTF-8; els accents no es veuran bé");

    ui_init();
    die_hook = ui_end;
    cmd_bind_defaults();

    e.last_input = now_seconds();
    while (!e.quit) {
        const Command *c;
        int key, confirm;

        autosave_tick(&e, now_seconds());
        render(&e);
        refresh();
        key_set_timeout(autosave_wait_ms(&e, now_seconds()));
        key = key_read();
        if (key < 0 || key == (KEY_FLAG_FN | KEY_RESIZE))
            continue;
        e.last_input = now_seconds();
        e.msg[0] = '\0';
        confirm = e.quit_confirm;
        c = key == KEY_PASTE ? NULL : cmd_for_key(key);
        if (c)
            ed_begin(&e, c->act, 0);
        else
            ed_begin(&e, key == KEY_PASTE ? ACT_NONE : ACT_TYPE, (wchar_t)key);

        if (key == KEY_PASTE) {
            size_t n;
            const char *text = key_paste_text(&n);
            char *conv;

            if (n > 0) {
                conv = buf_paste_text(&e.buf, e.cy, text, n, &n);
                ed_insert(&e, conv, n);
                free(conv);
            }
        } else if ((c = cmd_for_key(key))) {
            c->fn(&e);
        } else if (!(key & (KEY_FLAG_FN | KEY_FLAG_ALT)) && utf8_printable((wchar_t)key)) {
            char s[4];
            ed_insert(&e, s, utf8_encode((wchar_t)key, s));
        }
        ed_end(&e);
        /* L'avís de sortida només val per a la pulsació següent */
        if (e.quit_confirm == confirm)
            e.quit_confirm = 0;
    }

    ui_end();
    ed_free(&e);
    return 0;
}
