#include "config.h"
#include "theme.h"
#include "util.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

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
            snprintf(err, errsz, "tema desconegut «%s» (terminal, fosc, clar, sepia, fosfor o wordperfect)", v);
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
    } else if (!strcmp(key, "enfocament")) {
        if (!strcmp(v, "no"))
            e->focus = FOCUS_NO;
        else if (!strcmp(v, "paragraf") || !strcmp(v, "paràgraf"))
            e->focus = FOCUS_PARAGRAPH;
        else if (!strcmp(v, "frase"))
            e->focus = FOCUS_SENTENCE;
        else {
            snprintf(err, errsz, "«%s» ha de ser no, paragraf o frase", v);
            return -1;
        }
    } else if (!strcmp(key, "tipografia")) {
        return parse_bool(v, &e->typo, err, errsz);
    } else if (!strcmp(key, "recordar-posicio")) {
        return parse_bool(v, &e->remember_pos, err, errsz);
    } else if (!strcmp(key, "apostrof-tipografic")) {
        return parse_bool(v, &e->typo_apos, err, errsz);
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
    e->focus = d.focus;
    e->typo = d.typo;
    e->typo_apos = d.typo_apos;
    e->tw_mode = d.tw_mode;
    e->tw_limit = d.tw_limit;
    e->tw_anchor = d.tw_anchor;
    e->autosave = e->autosave_last = d.autosave;
    e->autosave_max = d.autosave_max;
    e->remember_pos = d.remember_pos;
    e->spell_start = d.spell_start;
    memcpy(e->palette, d.palette, sizeof e->palette);
    e->theme = d.theme;
    e->console_palette = d.console_palette;
    memcpy(e->langs, d.langs, sizeof e->langs);
    e->nlangs = d.nlangs;
    ed_free(&d);
}

static int load_all(Editor *e, const char *sys, const char *user, char *msg, size_t msgsz,
                    int with_overrides)
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
    for (int i = 0; with_overrides && i < noverrides; i++)
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

int config_load(Editor *e, const char *sys, const char *user, char *msg, size_t msgsz)
{
    return load_all(e, sys, user, msg, msgsz, 1);
}

int config_load_files(Editor *e, const char *sys, const char *user, char *msg, size_t msgsz)
{
    return load_all(e, sys, user, msg, msgsz, 0);
}

/* Clau d'una línia del fitxer ("" si és buida o un comentari) */
static void line_key(const char *line, char *key, size_t size)
{
    const char *s = line, *eq;
    size_t n;

    while (*s == ' ' || *s == '\t')
        s++;
    key[0] = '\0';
    if (*s == '#' || !(eq = strchr(s, '=')))
        return;
    n = (size_t)(eq - s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t'))
        n--;
    if (n < size) {
        memcpy(key, s, n);
        key[n] = '\0';
    }
}

int config_store(const char *path, const char *key, const char *value)
{
    char *text = NULL, *tmp, *dir, *slash, k[128];
    size_t len = 0, cap = 0, nl;
    const char *p, *at = NULL;
    int found = 0, err;
    FILE *f;

    if ((f = fopen(path, "r"))) {
        char buf[4096];

        while ((nl = fread(buf, 1, sizeof buf, f)) > 0) {
            if (len + nl + 1 > cap)
                text = xrealloc(text, cap = (len + nl + 1) * 2);
            memcpy(text + len, buf, nl);
            len += nl;
        }
        fclose(f);
    } else if (errno != ENOENT) {
        return -1;
    }
    if (!text)
        text = xmalloc(1);
    text[len] = '\0';

    dir = xstrdup(path);
    if ((slash = strrchr(dir, '/')) && slash != dir) {
        *slash = '\0';
        mkdir(dir, 0700);   /* si ja existeix, res */
    }
    free(dir);
    tmp = xmalloc(strlen(path) + 8);
    sprintf(tmp, "%s.nou", path);
    if (!(f = fopen(tmp, "w"))) {
        err = errno;
        free(text);
        free(tmp);
        errno = err;
        return -1;
    }
    /* Primer, on aniria si no hi és: abans del primer «color-» */
    for (p = text; *p; p = strchr(p, '\n') ? strchr(p, '\n') + 1 : p + strlen(p)) {
        line_key(p, k, sizeof k);
        if (!strcmp(k, key))
            found = 1;
        if (!at && !strncmp(k, "color-", 6))
            at = p;
    }
    for (p = text; *p; ) {
        const char *end = strchr(p, '\n');

        nl = end ? (size_t)(end - p) + 1 : strlen(p);
        line_key(p, k, sizeof k);
        if (!found && p == at)
            fprintf(f, "%s = %s\n", key, value);
        if (!strcmp(k, key))
            fprintf(f, "%s = %s\n", key, value);
        else
            fwrite(p, 1, nl, f);
        if (!end)
            fputc('\n', f);
        p += nl;
    }
    if (!found && !at)
        fprintf(f, "%s = %s\n", key, value);
    free(text);
    if (fflush(f) != 0 || fsync(fileno(f)) != 0) {
        err = errno;
        fclose(f);
        unlink(tmp);
        free(tmp);
        errno = err;
        return -1;
    }
    fclose(f);
    if (rename(tmp, path) < 0) {
        err = errno;
        unlink(tmp);
        free(tmp);
        errno = err;
        return -1;
    }
    free(tmp);
    return 0;
}

int config_save_all(const char *path, const Editor *e)
{
    char *old = NULL, *tmp, *bak, k[128];
    size_t len = 0, cap = 0, n;
    int kept = 0, err = 0;
    FILE *f;

    if ((f = fopen(path, "r"))) {
        char buf[4096];

        while ((n = fread(buf, 1, sizeof buf, f)) > 0) {
            if (len + n + 1 > cap)
                old = xrealloc(old, cap = (len + n + 1) * 2);
            memcpy(old + len, buf, n);
            len += n;
        }
        fclose(f);
        old[len] = '\0';
    } else if (errno != ENOENT) {
        return -1;
    }

    tmp = xmalloc(strlen(path) + 16);
    bak = xmalloc(strlen(path) + 16);
    sprintf(tmp, "%s.nou", path);
    sprintf(bak, "%s.anterior", path);
    if (old) {   /* còpia del fitxer anterior */
        if (!(f = fopen(bak, "w")) || fwrite(old, 1, len, f) != len || fclose(f) != 0)
            err = errno ? errno : EIO;
    }
    if (!err && !(f = fopen(tmp, "w")))
        err = errno;
    if (!err) {
        config_template(f, e);
        /* Les dreceres i les ordres externes del fitxer anterior */
        for (const char *p = old; p && *p; ) {
            const char *end = strchr(p, '\n');

            n = end ? (size_t)(end - p) + 1 : strlen(p);
            line_key(p, k, sizeof k);
            if (!strncmp(k, "tecla.", 6) || !strncmp(k, "externa", 7)) {
                if (!kept++)
                    fputs("\n# Del fitxer anterior\n", f);
                fwrite(p, 1, n, f);
                if (!end)
                    fputc('\n', f);
            }
            p += n;
        }
        if (fflush(f) != 0 || fsync(fileno(f)) != 0)
            err = errno;
        fclose(f);
        if (!err && rename(tmp, path) < 0)
            err = errno;
        if (err)
            unlink(tmp);
    }
    free(old);
    free(tmp);
    free(bak);
    errno = err;
    return err ? -1 : 0;
}

void config_template(FILE *f, const Editor *e)
{
    const short *th = theme_at((size_t)e->theme)->pal;
    char langs[MAX_LANGS * 17] = "";

    for (int i = 0; i < e->nlangs; i++) {
        strcat(langs, i ? " " : "");
        strcat(langs, e->langs[i]);
    }
    fprintf(f, "# Phileas: configuració\n"
            "#\n"
            "# Una opció per línia: clau = valor. Les línies que comencen per # són\n"
            "# comentaris. Els valors són els que hi havia en crear el fitxer (els de\n"
            "# Phileas, els del sistema i els de l'usuari); només cal deixar-hi les\n"
            "# opcions que es volen canviar.\n"
            "#\n"
            "# Es llegeix /etc/phileas/phileasrc i després el de l'usuari:\n"
            "# $PHILEAS_CONFIG/phileasrc, o bé ~/.config/phileas/phileasrc.\n"
            "# Les opcions de la línia d'ordres (-c, -a, -d) manen sobre tots dos.\n"
            "# Les opcions que es canvien des dels menús (centrar, colors, enfocament,\n"
            "# màquina d'escriure, tema i tipografia) es desen soles en aquest fitxer.\n"
            "\n"
            "# Text\n"
            "columnes = %-24d# amplada de la columna de text\n"
            "marge-esquerre = %-18d# columnes buides a l'esquerra\n"
            "marge-dret = %-22d# i a la dreta (si el terminal és estret)\n"
            "centrar = %-25s# centrar la columna entre els marges\n"
            "colors = %-26s# colors de Markdown\n"
            "enfocament = %-22s# no, paragraf o frase: atenua la resta del text\n"
            "\n"
            "# Tipografia en escriure: -- i un altre caràcter → —, << → «, >> → »,\n"
            "# ... → …; amb apostrof-tipografic, ' entre lletres → ’. No actua dins\n"
            "# del codi ni a les línies de guions (---). Ctrl+Z la desfà.\n"
            "tipografia = %s\n"
            "apostrof-tipografic = %s\n"
            "\n"
            "# Màquina d'escriure: salt, fixa o classic\n"
            "maquina-escriure = %s\n"
            "maquina-escriure-limit = %-10d# %% de l'alçada on salta el text\n"
            "maquina-escriure-ancoratge = %-6d# %% de l'alçada on queda el cursor\n"
            "\n"
            "# Desat\n"
            "autodesat = %-23d# segons sense escriure; 0 = desactivat\n"
            "autodesat-maxim = %-17d# segons com a molt amb canvis sense desar\n"
            "recordar-posicio = %-16s# en obrir un document, el cursor torna on era\n"
            "\n"
            "# Ortografia\n"
            "ortografia = %-22s# activa en arrencar (si no, F7)\n"
            "idiomes = %-25s# rotació de F8; el primer, en arrencar\n",
            e->columns, e->margin_left, e->margin_right, e->center ? "si" : "no",
            e->colors ? "si" : "no",
            e->focus == FOCUS_SENTENCE ? "frase" : e->focus == FOCUS_PARAGRAPH ? "paragraf" : "no",
            e->typo ? "si" : "no", e->typo_apos ? "si" : "no",
            e->tw_mode == TW_SALT ? "salt" : e->tw_mode == TW_FIXA ? "fixa" : "classic",
            e->tw_limit, e->tw_anchor, e->autosave, e->autosave_max,
            e->remember_pos ? "si" : "no",
            e->spell_start ? "si" : "no", langs);
    /* Sense «diccionari», config_load hi posa el primer idioma */
    if (e->dict[0] && strcmp(e->dict, e->langs[0]))
        fprintf(f, "diccionari = %-22s# un diccionari concret en arrencar\n", e->dict);
    else
        fputs("# diccionari = ca_ES-valencia      # un diccionari concret en arrencar\n", f);
    fprintf(f, "\n"
            "# Tema de colors: terminal (els colors del terminal), fosc, clar, sepia,\n"
            "# fosfor o wordperfect. També es canvia des del menú Vista o amb l'ordre «tema»,\n"
            "# que el deixen desat en aquesta línia.\n"
            "tema = %s\n"
            "paleta-consola = %s    # a la consola de Linux, els colors propis del tema\n"
            "\n"
            "# Colors que canvien els del tema (van després de la línia «tema»):\n"
            "# negre, vermell, verd, groc, blau, magenta, cian, blanc, gris,\n"
            "# vermell-clar, verd-clar, groc-clar, blau-clar, magenta-clar, cian-clar,\n"
            "# blanc-clar o cap (el del terminal). Sense colors, la barra i la\n"
            "# selecció van en vídeo invers. Els comentats són els del tema.\n",
            theme_at((size_t)e->theme)->name, e->console_palette ? "si" : "no");
    for (size_t i = 0; i < sizeof color_keys / sizeof *color_keys; i++) {
        int pal = color_keys[i].pal;

        fprintf(f, "%s%s = %s\n", e->palette[pal] == th[pal] ? "# " : "", color_keys[i].key,
                theme_color_name(e->palette[pal]));
    }
    fputs("\n"
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
          "# (Les del fitxer del sistema ja hi són; no cal repetir-les.)\n"
          "# externa.terminal = sh\n"
          "# externa-breu.paraules = wc -w\n",
          f);
}
