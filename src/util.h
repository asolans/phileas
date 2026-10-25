#ifndef PHILEAS_UTIL_H
#define PHILEAS_UTIL_H

#include <stddef.h>

/* S'executa abans d'acabar per un error fatal (p. ex. restaurar el terminal). */
extern void (*die_hook)(void);

void  die(const char *fmt, ...);
void *xmalloc(size_t n);
void *xrealloc(void *p, size_t n);
char *xstrdup(const char *s);

/* Directori de configuració de l'usuari: $PHILEAS_CONFIG, o bé
 * $XDG_CONFIG_HOME/phileas, o bé ~/.config/phileas. */
void  config_dir(char *out, size_t size);

#endif
