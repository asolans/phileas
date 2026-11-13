#ifndef COMU_RC_H
#define COMU_RC_H

#include <stddef.h>

/* Lectura del phileasrc (el de Phileas: «clau = valor», # comentaris).
 * Crida fn per a cada clau del fitxer del sistema (/etc/phileas/phileasrc) i
 * després del de l'usuari (~/.config/phileas/phileasrc), de manera que
 * l'usuari mana. */
typedef void (*cm_rc_fn)(const char *key, const char *value, void *ud);

void cm_rc_read(cm_rc_fn fn, void *ud);

#endif
