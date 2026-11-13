#ifndef COMU_CHOOSER_H
#define COMU_CHOOSER_H

#include <stddef.h>

/* Triador de pantalla sencera de Fàbula i Recerca (projectes, vaults): una
 * llista amb títol, ↑ ↓ Inici Fi per moure's, Retorn per triar i Esc o q per
 * sortir. El terminal ha d'estar iniciat (cm_look_start).
 *
 * items: els noms (n pot ser 0). extra: una opció més al final (p. ex.
 * «Projecte nou…»), o NULL. empty: el text que es mostra si no hi ha cap
 * element (amb extra, a sobre de l'opció). Retorna l'índex triat, n si és
 * l'opció extra, o -1 si es cancel·la. */
int cm_choose(const char *title, const char *const *items, size_t n, const char *extra,
              const char *empty);

/* Les subcarpetes de dir (sense les que comencen per punt) per ordre
 * alfabètic; si need no és NULL, només les que contenen aquest fitxer o
 * carpeta. Retorna el nombre i *out (alliberar amb cm_dirs_free). */
size_t cm_dirs(const char *dir, const char *need, char ***out);
void   cm_dirs_free(char **v, size_t n);

#endif
