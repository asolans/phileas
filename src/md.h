#ifndef PHILEAS_MD_H
#define PHILEAS_MD_H

#include <stddef.h>

/* Prefix de bloc Markdown d'una línia: sagnat, cites ("> ") i marcador de
 * llista ("- ", "* ", "+ ", "1. ", "1) "), amb la casella opcional de les
 * llistes de tasques ("[ ] ", "[x] "). Retorna la longitud en bytes (0 si
 * la línia no en té). */
size_t md_prefix(const char *s, size_t len);

/* Prefix de bloc sense comptar el sagnat inicial: 0 si la línia és només
 * text sagnat (o no té prefix). */
size_t md_marker(const char *s, size_t len);

/* Escriu a out el prefix que ha de tenir la línia següent en prémer Enter:
 * el mateix, amb el número de llista incrementat i la casella buida.
 * Retorna la longitud (truncada a outsz - 1). */
size_t md_continue(const char *s, size_t len, char *out, size_t outsz);

/* ---- Tipus de bloc ----
 * Cada línia del buffer és un bloc. Entre dos blocs hi ha una separació de
 * paràgraf (línia en blanc al fitxer, fila buida visual a la pantalla),
 * excepte si van «units»: elements d'una llista, línies d'una cita o d'una
 * taula, codi entre tanques, metadades i salts durs. */
enum {
    MD_EMPTY,       /* línia buida (fora de codi) */
    MD_TEXT,
    MD_HEADING,     /* # Capçalera */
    MD_LIST,
    MD_QUOTE,
    MD_TABLE,       /* | fila | */
    MD_HTML,
    MD_INDENT,      /* sagnat de 4 espais o més */
    MD_UNDERLINE,   /* === o --- sota un text (capçalera setext) */
    MD_FENCE_OPEN,  /* ``` o ~~~ */
    MD_FENCE_CLOSE,
    MD_CODE,        /* dins d'un bloc de codi */
    MD_META,        /* metadades YAML entre --- al principi del document */
};
#define MD_HARDBREAK 0x100   /* acaba en dos espais o '\\': salt dur */
#define MD_BASE(k)   ((k) & 0xFF)

/* Estat que passa d'una línia a la següent (blocs de codi i metadades). */
typedef struct {
    unsigned char fence_ch;   /* '`' o '~' dins d'un bloc de codi; 0 si no */
    unsigned char fence_len;
    unsigned char meta;       /* dins de les metadades */
    unsigned char started;    /* ja s'ha vist la primera línia */
} MdState;

/* Tipus de la línia s, que ve després de l'estat st (que s'actualitza). */
int md_kind(const char *s, size_t len, MdState *st);

/* 1 si els blocs consecutius k1 i k2 van units (sense separació). */
int md_tight(int k1, int k2);

/* 1 si, en carregar un fitxer, una línia k2 que segueix k1 sense línia en
 * blanc és la continuació del mateix paràgraf (text tallat a mà). */
int md_joinable(int k1, int k2);

/* ---- Notes al peu (sintaxi de pandoc) ----
 * Crida [^etiqueta] (sense espais ni claudàtors dins l'etiqueta) i
 * definició "[^etiqueta]: text" al principi d'un paràgraf. */

/* Si a s[i] comença una crida, la posició just després de ']'; si no, 0. */
size_t md_footnote(const char *s, size_t len, size_t i);

/* Si la línia és una definició, la posició del seu '['; si no, (size_t)-1. */
size_t md_footnote_def(const char *s, size_t len);

/* Primera crida a partir de from (el '['), saltant els fragments de codi i
 * els escapaments; (size_t)-1 si no n'hi ha. */
size_t md_next_footnote(const char *s, size_t len, size_t from);

/* ---- Estils per als colors ----
 * Cada byte d'una línia rep una combinació d'aquests bits, que render.c
 * converteix en atributs del terminal. */
#define MS_MARK    0x01   /* marcador de sintaxi (#, **, -, >...): atenuat */
#define MS_BOLD    0x02
#define MS_ITALIC  0x04
#define MS_CODE    0x08
#define MS_LINK    0x10   /* text d'un enllaç */
#define MS_QUOTE   0x20
#define MS_H(n)    ((n) << 6)          /* capçalera de nivell 1-3 (4-6 = 3) */
#define MS_HLEVEL(st) ((st) >> 6)

/* Omple out[0..len) amb l'estil de cada byte de la línia s, de tipus kind.
 * setext és el nivell (1 o 2) si la línia és el text d'una capçalera
 * subratllada amb === o ---, i 0 si no. */
void md_style(const char *s, size_t len, int kind, int setext, unsigned char *out);

#endif
