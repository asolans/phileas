#include "config.h"
#include "theme.h"
#include "util.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

int (*config_bind_hook)(const char *key, const char *cmd);
int (*config_extern_hook)(const char *name, const char *shell, int brief, char *err, size_t errsz);

/* Nom d'ordre: lletres minúscules, xifres i guionets */
static int valid_command_name(const char *s)
{
    if (!*s || strlen(s) >= 32)
        return 0;
    for (; *s; s++)
        if (!((*s >= 'a' && *s <= 'z') || (*s >= '0' && *s <= '9') || *s == '-'))
            return 0;
    return 1;
}

/* Opcions de la línia d'ordres, en l'ordre en què s'han donat */
#define MAX_OVERRIDES 16
static struct {
    char key[32], value[64];
} overrides[MAX_OVERRIDES];
static int noverrides;

void config_override(const char *key, const char *value)
{
    if (noverrides == MAX_OVERRIDES)
        return;
    snprintf(overrides[noverrides].key, sizeof overrides[0].key, "%s", key);
    snprintf(overrides[noverrides].value, sizeof overrides[0].value, "%s", value);
    noverrides++;
}

void config_user_path(char *out, size_t size)
{
    char dir[1024];

    config_dir(dir, sizeof dir);
    snprintf(out, size, "%s/phileasrc", dir);
}

/* ---- Valors ---- */

static int parse_int(const char *v, int min, int max, int *out, char *err, size_t errsz)
{
    char *end;
    long n;

    errno = 0;
    n = strtol(v, &end, 10);
    if (!*v || *end || errno || n < min || n > max) {
        snprintf(err, errsz, "«%s» ha de ser un número entre %d i %d", v, min, max);
        return -1;
    }
    *out = (int)n;
    return 0;
}

static int parse_bool(const char *v, int *out, char *err, size_t errsz)
{
    if (!strcasecmp(v, "si") || !strcasecmp(v, "sí") || !strcmp(v, "1")) {
        *out = 1;
        return 0;
    }
    if (!strcasecmp(v, "no") || !strcmp(v, "0")) {
        *out = 0;
        return 0;
    }
    snprintf(err, errsz, "«%s» ha de ser si o no", v);
    return -1;
}

static int parse_color(const char *v, short *out, char *err, size_t errsz)
{
    short c = theme_color_parse(v);

    if (c == -2) {
        snprintf(err, errsz, "color desconegut «%s» (negre, vermell, verd, groc, blau, magenta, "
                 "cian, blanc, gris, les clares com verd-clar, o cap)", v);
        return -1;
    }
    *out = c;
    return 0;
}

static const struct {
    const char *key;
    int pal;
} color_keys[] = {
    { "color-capcalera-1", PAL_H1 }, { "color-capcalera-2", PAL_H2 },
    { "color-capcalera-3", PAL_H3 }, { "color-codi", PAL_CODE },
    { "color-cita", PAL_QUOTE },     { "color-error", PAL_ERROR },
    { "color-marcadors", PAL_MARK },
    { "color-text", PAL_TEXT },      { "color-fons", PAL_BG },
    { "color-barra", PAL_BAR },      { "color-barra-fons", PAL_BAR_BG },
    { "color-seleccio", PAL_SEL },   { "color-seleccio-fons", PAL_SEL_BG },
    { "color-menu", PAL_MENU },      { "color-menu-fons", PAL_MENU_BG },
};

int config_set(Editor *e, const char *key, const char *v, char *err, size_t errsz)
{
    int n;

    if (!strncmp(key, "tecla.", 6)) {
        if (!key[6]) {
            snprintf(err, errsz, "falta el nom de la tecla");
            return -1;
        }
        if (config_bind_hook && config_bind_hook(key + 6, v) < 0) {
            snprintf(err, errsz, "tecla «%s» o ordre «%s» desconeguda", key + 6, v);
            return -1;
        }
        return 0;
    }
    if (!strncmp(key, "externa.", 8) || !strncmp(key, "externa-breu.", 13)) {
        int brief = key[7] == '-';
        const char *name = key + (brief ? 13 : 8);

        if (!valid_command_name(name)) {
            snprintf(err, errsz, "nom d'ordre «%s» incorrecte (lletres minúscules, xifres i "
                     "guionets)", name);
            return -1;
        }
        if (!*v) {
            snprintf(err, errsz, "falta l'ordre que executa «%s»", name);
            return -1;
        }
        if (config_extern_hook)
            return config_extern_hook(name, v, brief, err, errsz);
        return 0;
    }
    for (size_t i = 0; i < sizeof color_keys / sizeof *color_keys; i++)
        if (!strcmp(key, color_keys[i].key))
            return parse_color(v, &e->palette[color_keys[i].pal], err, errsz);

    if (!strcmp(key, "tema")) {
        const Theme *t = theme_find(v);

        if (!t) {
            snprintf(err, errsz, "tema desconegut «%s» (terminal, fosc, clar, sepia o fosfor)", v);
            return -1;
        }
        theme_apply(e, t);
        return 0;
    }
    if (!strcmp(key, "paleta-consola"))
        return parse_bool(v, &e->console_palette, err, errsz);

    if (!strcmp(key, "columnes")) {
        if (parse_int(v, 10, 1000, &n, err, errsz) < 0)
            return -1;
        e->columns = n;
    } else if (!strcmp(key, "marge-esquerre")) {
        return parse_int(v, 0, 100, &e->margin_left, err, errsz);
    } else if (!strcmp(key, "marge-dret")) {
        return parse_int(v, 0, 100, &e->margin_right, err, errsz);
    } else if (!strcmp(key, "centrar")) {
        return parse_bool(v, &e->center, err, errsz);
    } else if (!strcmp(key, "colors")) {
        return parse_bool(v, &e->colors, err, errsz);
    } else if (!strcmp(key, "maquina-escriure")) {
        if (!strcmp(v, "salt"))
            e->tw_mode = TW_SALT;
        else if (!strcmp(v, "fixa"))
            e->tw_mode = TW_FIXA;
        else if (!strcmp(v, "classic") || !strcmp(v, "clàssic"))
            e->tw_mode = TW_CLASSIC;
        else {
            snprintf(err, errsz, "«%s» ha de ser salt, fixa o classic", v);
            return -1;
        }
    } else if (!strcmp(key, "maquina-escriure-limit")) {
        return parse_int(v, 10, 100, &e->tw_limit, err, errsz);
    } else if (!strcmp(key, "maquina-escriure-ancoratge")) {
        return parse_int(v, 0, 100, &e->tw_anchor, err, errsz);
    } else if (!strcmp(key, "autodesat")) {
        if (parse_int(v, 0, 86400, &n, err, errsz) < 0)
            return -1;
        e->autosave = n;
        if (n)
            e->autosave_last = n;
    } else if (!strcmp(key, "autodesat-maxim")) {
        return parse_int(v, 10, 86400, &e->autosave_max, err, errsz);
    } else if (!strcmp(key, "ortografia")) {
        return parse_bool(v, &e->spell_start, err, errsz);
    } else if (!strcmp(key, "diccionari")) {
        if (!*v || strlen(v) >= sizeof e->dict || strchr(v, '/')) {
            snprintf(err, errsz, "nom de diccionari no vàlid «%s»", v);
            return -1;
        }
        snprintf(e->dict, sizeof e->dict, "%s", v);
    } else if (!strcmp(key, "idiomes")) {
        char tmp[128], *tok, *save;
        int k = 0;

        snprintf(tmp, sizeof tmp, "%s", v);
        for (tok = strtok_r(tmp, " \t,", &save); tok; tok = strtok_r(NULL, " \t,", &save)) {
            if (k == MAX_LANGS || strlen(tok) >= sizeof e->langs[0]) {
                snprintf(err, errsz, "com a molt %d idiomes, de %zu lletres", MAX_LANGS,
                         sizeof e->langs[0] - 1);
                return -1;
            }
            snprintf(e->langs[k++], sizeof e->langs[0], "%s", tok);
        }
        if (k == 0) {
            snprintf(err, errsz, "cal com a mínim un idioma");
            return -1;
        }
        e->nlangs = k;
    } else {
        snprintf(err, errsz, "opció desconeguda «%s»", key);
        return -1;
    }
    return 0;
}

/* ---- Fitxers ---- */

static char *trim(char *s)
{
    char *end;

    while (*s == ' ' || *s == '\t')
        s++;
    end = s + strlen(s);
    while (end > s && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n'))
        *--end = '\0';
    return s;
}

static void note_error(int *nerr, char *msg, size_t msgsz, const char *file, int line,
                       const char *err)
{
    if (++*nerr == 1) {
        const char *base = strrchr(file, '/');

        if (line > 0)
            snprintf(msg, msgsz, "%s, línia %d: %s", base ? base + 1 : file, line, err);
        else
            snprintf(msg, msgsz, "%s: %s", file, err);
    }
}

static void load_file(Editor *e, const char *path, int *nerr, char *msg, size_t msgsz)
{
    FILE *f = fopen(path, "r");
    char buf[512], err[256];
    int line = 0;

    if (!f) {
        if (errno != ENOENT)
            note_error(nerr, msg, msgsz, path, 0, strerror(errno));
        return;
    }
    while (fgets(buf, sizeof buf, f)) {
        char *s, *eq, *hash, *key, *value;

        line++;
        /* Comentaris: # al principi o després d'un espai */
        for (hash = buf; (hash = strchr(hash, '#'));) {
            if (hash == buf || hash[-1] == ' ' || hash[-1] == '\t') {
                *hash = '\0';
                break;
            }
            hash++;
        }
        s = trim(buf);
        if (!*s)
            continue;
        if (!(eq = strchr(s, '='))) {
            note_error(nerr, msg, msgsz, path, line, "falta «=»");
            continue;
        }
        *eq = '\0';
        key = trim(s);
        value = trim(eq + 1);
        if (config_set(e, key, value, err, sizeof err) < 0)
            note_error(nerr, msg, msgsz, path, line, err);
    }
    fclose(f);
}

/* Torna als valors interns les opcions configurables (per recarregar) */
static void defaults(Editor *e)
{
    Editor d;

    ed_init(&d, 72);
    e->columns = d.columns;
    e->center = d.center;
    e->margin_left = d.margin_left;
    e->margin_right = d.margin_right;
    e->colors = d.colors;
    e->tw_mode = d.tw_mode;
    e->tw_limit = d.tw_limit;
    e->tw_anchor = d.tw_anchor;
    e->autosave = e->autosave_last = d.autosave;
    e->autosave_max = d.autosave_max;
    e->spell_start = d.spell_start;
    memcpy(e->palette, d.palette, sizeof e->palette);
    e->theme = d.theme;
    e->console_palette = d.console_palette;
    memcpy(e->langs, d.langs, sizeof e->langs);
    e->nlangs = d.nlangs;
    ed_free(&d);
}

int config_load(Editor *e, const char *sys, const char *user, char *msg, size_t msgsz)
{
    char path[1100], err[256];
    int nerr = 0;

    msg[0] = '\0';
    defaults(e);
    if (sys)
        load_file(e, sys, &nerr, msg, msgsz);
    if (!user) {
        config_user_path(path, sizeof path);
        user = path;
    }
    load_file(e, user, &nerr, msg, msgsz);
    for (int i = 0; i < noverrides; i++)
        if (config_set(e, overrides[i].key, overrides[i].value, err, sizeof err) < 0)
            note_error(&nerr, msg, msgsz, "línia d'ordres", 0, err);
    /* Sense «diccionari», l'ortografia comença amb el primer idioma */
    if (!e->dict[0])
        snprintf(e->dict, sizeof e->dict, "%s", e->langs[0]);
    if (nerr > 1) {
        size_t n = strlen(msg);

        snprintf(msg + n, msgsz - n, nerr == 2 ? " (i 1 error més)" : " (i %d errors més)",
                 nerr - 1);
    }
    return nerr;
}

void config_template(FILE *f)
{
    fputs("# Phileas: configuració\n"
          "#\n"
          "# Una opció per línia: clau = valor. Les línies que comencen per # són\n"
          "# comentaris. Aquest fitxer té els valors per defecte; només cal deixar-hi\n"
          "# les opcions que es volen canviar.\n"
          "#\n"
          "# Es llegeix /etc/phileas/phileasrc i després el de l'usuari:\n"
          "# $PHILEAS_CONFIG/phileasrc, o bé ~/.config/phileas/phileasrc.\n"
          "# Les opcions de la línia d'ordres (-c, -a, -d) manen sobre tots dos.\n"
          "\n"
          "# Text\n"
          "columnes = 72                      # amplada de la columna de text\n"
          "marge-esquerre = 4                 # columnes buides a l'esquerra\n"
          "marge-dret = 4                     # i a la dreta (si el terminal és estret)\n"
          "centrar = no                       # centrar la columna entre els marges\n"
          "colors = si                        # colors de Markdown\n"
          "\n"
          "# Màquina d'escriure: salt, fixa o classic\n"
          "maquina-escriure = salt\n"
          "maquina-escriure-limit = 80        # % de l'alçada on salta el text\n"
          "maquina-escriure-ancoratge = 50    # % de l'alçada on queda el cursor\n"
          "\n"
          "# Desat\n"
          "autodesat = 30                     # segons sense escriure; 0 = desactivat\n"
          "autodesat-maxim = 300              # segons com a molt amb canvis sense desar\n"
          "\n"
          "# Ortografia\n"
          "ortografia = no                    # activa en arrencar (si no, F7)\n"
          "idiomes = ca es                    # rotació de F8; el primer, en arrencar\n"
          "# diccionari = ca_ES-valencia      # un diccionari concret en arrencar\n"
          "\n"
          "# Tema de colors: terminal (els colors del terminal), fosc, clar, sepia\n"
          "# o fosfor. També es canvia des del menú Vista o amb l'ordre «tema».\n"
          "tema = terminal\n"
          "paleta-consola = si    # a la consola de Linux, els colors propis del tema\n"
          "\n"
          "# Colors que canvien els del tema (van després de la línia «tema»):\n"
          "# negre, vermell, verd, groc, blau, magenta, cian, blanc, gris,\n"
          "# vermell-clar, verd-clar, groc-clar, blau-clar, magenta-clar, cian-clar,\n"
          "# blanc-clar o cap (el del terminal). Sense colors, la barra i la\n"
          "# selecció van en vídeo invers.\n"
          "# color-capcalera-1 = magenta\n"
          "# color-capcalera-2 = cian\n"
          "# color-capcalera-3 = verd\n"
          "# color-codi = groc\n"
          "# color-cita = verd\n"
          "# color-error = vermell\n"
          "# color-marcadors = cap      # els signes de Markdown (cap: atenuats)\n"
          "# color-text = cap\n"
          "# color-fons = cap\n"
          "# color-barra = cap\n"
          "# color-barra-fons = cap\n"
          "# color-seleccio = cap\n"
          "# color-seleccio-fons = cap\n"
          "# color-menu = cap\n"
          "# color-menu-fons = cap\n"
          "\n"
          "# Dreceres: tecla.NOM = ordre, o tecla.NOM = (buit) per treure-la.\n"
          "# Noms: ctrl-a ... ctrl-z, alt-LLETRA, f1 ... f12, shift-f1 ... shift-f12,\n"
          "# up, down, left, right, home, end, pgup, pgdn, delete, insert, amb\n"
          "# shift-, ctrl-, alt- o ctrl-shift- davant. La paleta (Ctrl+P) té els noms\n"
          "# de les ordres.\n"
          "# tecla.ctrl-t = maquina-escriure\n"
          "# tecla.f9 = colors\n"
          "\n"
          "# Ordres externes: NOM passa a ser una ordre de la paleta que executa\n"
          "# ORDRE amb /bin/sh. A la paleta, el que s'escriu després del nom en són\n"
          "# els arguments. Abans d'executar-la es desa el document si té canvis.\n"
          "#   externa.NOM = ORDRE       amb la pantalla del terminal; Retorn torna\n"
          "#   externa-breu.NOM = ORDRE  la darrera línia de la sortida, a la barra\n"
          "# Per assignar-hi una tecla, la línia tecla. va després de la definició.\n"
          "# externa.terminal = sh\n"
          "# externa-breu.paraules = wc -w\n",
          f);
}
