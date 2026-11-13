#ifndef COMU_RC_H
#define COMU_RC_H

#include <stddef.h>

/* Lectura del phileasrc (el de Phileas: «clau = valor», # comentaris).
 * Crida fn per a cada clau del fitxer del sistema (/etc/phileas/phileasrc) i
 * després del de l'usuari (~/.config/phileas/phileasrc), de manera que
 * l'usuari mana. */
typedef void (*cm_rc_fn)(const char *key, const char *value, void *ud);

void cm_rc_read(cm_rc_fn fn, void *ud);

#include "palette.h"

/* Colors que canvien els del tema, com a Phileas («color-barra = blau»…).
 * over[] comença amb CM_NO_COLOR (cap canvi). Si key és una d'aquestes
 * claus, hi posa el color i retorna 1. Una línia «tema» posterior els
 * esborra (cm_rc_color_reset), com a Phileas. */
#define CM_NO_COLOR (-2)
int  cm_rc_color(const char *key, const char *value, short over[PAL_N]);
void cm_rc_color_reset(short over[PAL_N]);
/* pal = la del tema amb els canvis de over */
void cm_rc_apply(short pal[PAL_N], const short over[PAL_N]);

#endif
