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

#endif
