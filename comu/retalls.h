#ifndef COMU_RETALLS_H
#define COMU_RETALLS_H

#include <stddef.h>
#include <sys/stat.h>

/* Porta-retalls comú de Phileas, Fàbula i Recerca: un fitxer, «retalls», al
 * directori de configuració (a la BYOK, /data/phileas/retalls). Hi va el text
 * copiat en el format del fitxer (Markdown). Serveix entre programes i entre
 * sessions, també a la consola, on no hi ha porta-retalls del sistema.
 *
 * st (pot ser NULL) rep l'estat del fitxer: qui l'ha escrit el recorda per
 * saber, en enganxar, si després hi ha escrit un altre programa. */

/* Hi desa el text (de manera atòmica). 0, o -1 amb errno. */
int   cm_retalls_write(const char *s, size_t n, struct stat *st);
/* El text (cal alliberar-lo), o NULL si no n'hi ha. */
char *cm_retalls_read(size_t *n, struct stat *st);
/* 1 si el fitxer és diferent del que diu st (l'ha canviat algú altre) */
int   cm_retalls_changed(const struct stat *st);

#endif
