#include "theme.h"

#include <string.h>
#include <strings.h>

#define C PAL_DEFAULT

/* Colors: 0 negre, 1 vermell, 2 verd, 3 groc, 4 blau, 5 magenta, 6 cian,
 * 7 blanc i, de 8 a 15, les versions clares. A la consola de Linux el fons
 * només pot ser de 0 a 7, i la negreta fa servir la versió clara del color
 * (les capçaleres d'un tema amb rgb surten dels colors 8-15). */

/* Fosc: colors suaus sobre gris molt fosc */
static const unsigned rgb_fosc[16] = {
    0x1d1f21, 0xcc6666, 0xb5bd68, 0xf0c674, 0x34506a, 0xb294bb, 0x8abeb7, 0xc5c8c6,
    0x7a7c7a, 0xd54e53, 0xb9ca4a, 0xe7c547, 0x81a2be, 0xc397d8, 0x70c0b1, 0xeaeaea,
};

/* Clar: tinta fosca sobre blanc trencat. El 3 (groc, que el tema no fa
 * servir) és el gris del text atenuat. */
static const unsigned rgb_clar[16] = {
    0x2a2a2a, 0xb03030, 0x3a7a30, 0x9a9a94, 0x2c5aa0, 0x8a3a8a, 0x2a7a8a, 0xf5f5f0,
    0x000000, 0xc02020, 0x2a6a20, 0x7a5a00, 0x1c4a90, 0x7a2a7a, 0x1a6a7a, 0xffffff,
};

/* Sèpia: tinta marró sobre paper crema */
static const unsigned rgb_sepia[16] = {
    0x433422, 0xa0392b, 0x5a6b2f, 0xe8d5a8, 0x2f4f6f, 0xa89878, 0x7b5b33, 0xf4ecd8,
    0x1f160c, 0x8b3a1a, 0x4d5e22, 0xd8c598, 0x284566, 0x988868, 0x6b4b23, 0xfffaf0,
};

/* Fòsfor: pantalla verda d'ordinador antic */
static const unsigned rgb_fosfor[16] = {
    0x0a120a, 0xff5555, 0x33dd55, 0xffb000, 0x0a3a1a, 0x1f7a33, 0x33dd55, 0x33dd55,
    0x1f5f2f, 0xff7777, 0x88ffaa, 0xffc840, 0x1a5a2a, 0x2f9a43, 0x88ffaa, 0x88ffaa,
};

/* WordPerfect 5.1 per a DOS: la paleta CGA, text gris sobre blau. El 5
 * (magenta, que el tema no fa servir) és un gris blavós per al text atenuat:
 * el gris fosc de la CGA no es llegeix sobre el blau. */
static const unsigned rgb_wp[16] = {
    0x000000, 0xaa0000, 0x00aa00, 0xaa5500, 0x0000aa, 0x7d7dd0, 0x00aaaa, 0xaaaaaa,
    0x555555, 0xff5555, 0x55ff55, 0xffff55, 0x5555ff, 0xff55ff, 0x55ffff, 0xffffff,
};

static const Theme themes[] = {
    /* El de sempre: els colors del terminal, barres en vídeo invers */
    { "terminal", "Terminal", {
        [PAL_H1] = 5, [PAL_H2] = 6, [PAL_H3] = 2, [PAL_CODE] = 3, [PAL_QUOTE] = 2,
        [PAL_ERROR] = 1, [PAL_MARK] = C, [PAL_TEXT] = C, [PAL_BG] = C,
        [PAL_BAR] = C, [PAL_BAR_BG] = C, [PAL_SEL] = C, [PAL_SEL_BG] = C,
        [PAL_MENU] = C, [PAL_MENU_BG] = C }, NULL, -1 },
    { "fosc", "Fosc", {
        [PAL_H1] = 5, [PAL_H2] = 6, [PAL_H3] = 2, [PAL_CODE] = 3, [PAL_QUOTE] = 2,
        [PAL_ERROR] = 1, [PAL_MARK] = C, [PAL_TEXT] = 7, [PAL_BG] = 0,
        [PAL_BAR] = 0, [PAL_BAR_BG] = 6, [PAL_SEL] = 7, [PAL_SEL_BG] = 4,
        [PAL_MENU] = C, [PAL_MENU_BG] = C }, rgb_fosc, 8 },
    { "clar", "Clar", {
        [PAL_H1] = 4, [PAL_H2] = 2, [PAL_H3] = 6, [PAL_CODE] = 5, [PAL_QUOTE] = 2,
        [PAL_ERROR] = 1, [PAL_MARK] = C, [PAL_TEXT] = 0, [PAL_BG] = 7,
        [PAL_BAR] = 7, [PAL_BAR_BG] = 4, [PAL_SEL] = 7, [PAL_SEL_BG] = 4,
        [PAL_MENU] = C, [PAL_MENU_BG] = C }, rgb_clar, 3 },
    { "sepia", "Sèpia", {
        [PAL_H1] = 1, [PAL_H2] = 4, [PAL_H3] = 2, [PAL_CODE] = 6, [PAL_QUOTE] = 2,
        [PAL_ERROR] = 1, [PAL_MARK] = 5, [PAL_TEXT] = 0, [PAL_BG] = 7,
        [PAL_BAR] = 7, [PAL_BAR_BG] = 0, [PAL_SEL] = 0, [PAL_SEL_BG] = 3,
        [PAL_MENU] = C, [PAL_MENU_BG] = C }, rgb_sepia, 5 },
    { "fosfor", "Fòsfor", {
        [PAL_H1] = 2, [PAL_H2] = 2, [PAL_H3] = 2, [PAL_CODE] = 3, [PAL_QUOTE] = 2,
        [PAL_ERROR] = 1, [PAL_MARK] = C, [PAL_TEXT] = 2, [PAL_BG] = 0,
        [PAL_BAR] = 0, [PAL_BAR_BG] = 2, [PAL_SEL] = 0, [PAL_SEL_BG] = 2,
        [PAL_MENU] = C, [PAL_MENU_BG] = C }, rgb_fosfor, 5 },
    /* Capçaleres en negreta: blanc, groc i cian brillants. Marques en gris
     * fosc; barra i menús grisos com els programes de DOS. */
    { "wordperfect", "WordPerfect", {
        [PAL_H1] = 7, [PAL_H2] = 3, [PAL_H3] = 6, [PAL_CODE] = 10, [PAL_QUOTE] = 6,
        [PAL_ERROR] = 9, [PAL_MARK] = 8, [PAL_TEXT] = 7, [PAL_BG] = 4,
        [PAL_BAR] = 0, [PAL_BAR_BG] = 7, [PAL_SEL] = 15, [PAL_SEL_BG] = 0,
        [PAL_MENU] = 0, [PAL_MENU_BG] = 7 }, rgb_wp, 5 },
};
#define NTHEMES (sizeof themes / sizeof themes[0])

size_t theme_count(void)
{
    return NTHEMES;
}

const Theme *theme_at(size_t i)
{
    return i < NTHEMES ? &themes[i] : NULL;
}

const Theme *theme_find(const char *name)
{
    for (size_t i = 0; i < NTHEMES; i++)
        if (!strcasecmp(name, themes[i].name))
            return &themes[i];
    return NULL;
}

void theme_apply(Editor *e, const Theme *t)
{
    memcpy(e->palette, t->pal, sizeof e->palette);
    e->theme = (int)(t - themes);
}

static const char *const color_names[16] = {
    "negre", "vermell", "verd", "groc", "blau", "magenta", "cian", "blanc",
    "gris", "vermell-clar", "verd-clar", "groc-clar", "blau-clar", "magenta-clar",
    "cian-clar", "blanc-clar",
};

const char *theme_color_name(short c)
{
    return c >= 0 && c < 16 ? color_names[c] : "cap";
}

short theme_color_parse(const char *name)
{
    if (!strcasecmp(name, "cap"))
        return PAL_DEFAULT;
    for (short i = 0; i < 16; i++)
        if (!strcasecmp(name, color_names[i]))
            return i;
    return -2;
}
