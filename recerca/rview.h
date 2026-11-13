#ifndef RECERCA_RVIEW_H
#define RECERCA_RVIEW_H

#include "vault.h"

/* Opcions de la pantalla (del phileasrc) */
typedef struct {
    int         replica;    /* el vault és una rèplica del PC (la Pi): només
                             * s'escriu a la carpeta d'entrada */
    const char *entrada;    /* carpeta de les fitxes escrites aquí («_pi») */
} RviewOpts;

/* La pantalla de Recerca: navegació (carpetes, etiquetes o tipus), llista
 * de fitxes i visor amb els enllaços i les notes que hi enllacen; edició a
 * Phileas, fitxes noves, anotacions i paperera. Cal haver cridat ft_init. */
void rview_run(Vault *v, const RviewOpts *o);

#endif
