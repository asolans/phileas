#include "autosave.h"
#include "command.h"
#include "config.h"
#include "editor.h"
#include "history.h"
#include "keys.h"
#include "render.h"
#include "spell.h"
#include "tui.h"
#include "utf8.h"
#include "util.h"
#include "version.h"

#include <errno.h>
#include <langinfo.h>
#include <limits.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DEFAULT_COLUMNS  72

static Editor *editor;   /* per al desat d'emergència */

/* SIGHUP (s'ha tancat el terminal) o SIGTERM (p. ex. l'apagada de la BYOK):
 * es desa el document i se surt. */
static void quit_on_signal(int sig)
{
    char path[1100];
    int r = autosave_emergency(editor, path, sizeof path);

    ui_end();
    if (r < 0)
        fprintf(stderr, "phileas: %s: no s'ha pogut desar el document\n", strsignal(sig));
    else
        history_remember(editor);
    if (r >= 0 && path[0])
        fprintf(stderr, "phileas: %s: document desat a %s\n", strsignal(sig), path);
    _exit(128 + sig);
}

static void usage(FILE *f)
{
    fputs("Ús: phileas [-c COLUMNES] [-a SEGONS] [-d IDIOMA] [FITXER]\n"
          "     phileas --config\n"
          "\n"
          "Si FITXER no existeix i no té extensió, es crea amb .md (nota → nota.md).\n"
          "\n"
          "  -c N      amplada del text en columnes (per defecte 72)\n"
          "  -a N      desa sol després de N segons sense escriure (per defecte 30;\n"
          "            0 el desactiva). Desa sobre el mateix fitxer.\n"
          "  -d ID     activa l'ortografia amb aquest diccionari: ca, es, en_US...\n"
          "            (-d no la desactiva). Sense -d, F7 l'activa i F8 alterna ca i es.\n"
          "  --config  escriu la configuració completa amb els valors actuals (no la\n"
          "            redirigiu al mateix phileasrc: el shell el buida abans de llegir-lo;\n"
          "            per desar-la, dins Phileas, Ctrl+P desar-configuracio)\n"
          "  -h        mostra aquesta ajuda\n"
          "  -v        mostra la versió\n"
          "\n"
          "Configuració: /etc/phileas/phileasrc i després $PHILEAS_CONFIG/phileasrc\n"
          "(o ~/.config/phileas/phileasrc). Les opcions -c, -a i -d manen sobre tots dos.\n"
          "Al mateix directori, «historial»: els documents recents (Alt+O) i on era el\n"
          "cursor, que hi torna en obrir-los (recordar-posicio = no ho desactiva). Els\n"
          "documents no es modifiquen.\n",
          f);
}

/* Una opció de la línia d'ordres: es comprova ara (si és incorrecta, no
 * s'arrenca) i s'aplica després dels fitxers de configuració. */
static void cli_option(const char *key, const char *value)
{
    Editor scratch;
    char err[256];
    int bad;

    ed_init(&scratch, DEFAULT_COLUMNS);
    bad = config_set(&scratch, key, value, err, sizeof err) < 0;
    ed_free(&scratch);
    if (bad) {
        fprintf(stderr, "phileas: %s: %s\n", key, err);
        exit(2);
    }
    config_override(key, value);
}

int main(int argc, char **argv)
{
    Editor e;
    char cfg_msg[256];
    int opt, cfg_errors;

    setlocale(LC_ALL, "");
    /* La configuració completa amb els valors actuals (els del sistema i
     * els de l'usuari), per fer-se un phileasrc sense perdre'n cap */
    if (argc > 1 && !strcmp(argv[1], "--config")) {
        ed_init(&e, 72);
        if (config_load_files(&e, CONFIG_SYSTEM, NULL, cfg_msg, sizeof cfg_msg))
            fprintf(stderr, "phileas: configuració: %s\n", cfg_msg);
        config_template(stdout, &e);
        ed_free(&e);
        return 0;
    }
    while ((opt = getopt(argc, argv, "a:c:d:hv")) != -1) {
        switch (opt) {
        case 'c':
            cli_option("columnes", optarg);
            break;
        case 'a':
            cli_option("autodesat", optarg);
            break;
        case 'd':
            if (!strcmp(optarg, "no")) {
                cli_option("ortografia", "no");
            } else {
                cli_option("diccionari", optarg);
                cli_option("ortografia", "si");
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

    ed_init(&e, DEFAULT_COLUMNS);
    cmd_bind_defaults();
    config_bind_hook = cmd_config_bind;
    config_extern_hook = cmd_config_extern;
    cfg_errors = config_load(&e, CONFIG_SYSTEM, NULL, cfg_msg, sizeof cfg_msg);

    if (optind < argc) {
        char path[PATH_MAX];

        buf_new_path(argv[optind], path, sizeof path);

        if (ed_open(&e, path) < 0) {
            fprintf(stderr, "phileas: %s: %s\n", path, strerror(errno));
            ed_free(&e);
            return 1;
        }
        history_restore(&e);
        history_remember(&e);
    }
    if (strcmp(nl_langinfo(CODESET), "UTF-8") != 0)
        ed_msg(&e, "Avís: la configuració regional no és UTF-8; els accents no es veuran bé");
    if (cfg_errors)
        ed_msg(&e, "Configuració: %s", cfg_msg);

    ui_init();
    die_hook = ui_end;
    editor = &e;
    key_quit_hook = quit_on_signal;
    key_signals();
    render_colors(&e);

    /* L'ortografia només s'activa en arrencar si es demana (-d o l'opció
     * «ortografia»; si no, amb F7). El diccionari triga una mica a
     * carregar-se (segons, en una Raspberry Pi petita): primer es mostra el
     * document. */
    if (e.spell_start) {
        char msg[sizeof e.msg];

        snprintf(msg, sizeof msg, "%s", e.msg);
        ed_msg(&e, "Carregant el diccionari…");
        render(&e);
        refresh();
        e.spell = spell_open(e.dict) == 0;
        if (e.spell) {
            snprintf(e.dict_loaded, sizeof e.dict_loaded, "%s", e.dict);
            snprintf(e.msg, sizeof e.msg, "%s", msg);
        } else {
            ed_msg(&e, "Ortografia desactivada: %s", spell_error());
        }
    }

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
            cmd_run(&e, c, NULL);
        } else if (!(key & (KEY_FLAG_FN | KEY_FLAG_ALT)) && utf8_printable((wchar_t)key)) {
            char s[4];
            ed_insert(&e, s, utf8_encode((wchar_t)key, s));
            ed_typography(&e, (wchar_t)key);
        }
        ed_end(&e);
        /* L'avís de sortida només val per a la pulsació següent */
        if (e.quit_confirm == confirm)
            e.quit_confirm = 0;
    }

    history_remember(&e);
    ui_end();
    spell_close();
    cmd_extern_clear();
    ed_free(&e);
    return 0;
}
