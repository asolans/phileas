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

#endif
