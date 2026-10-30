#ifndef PHILEAS_CLIPBOARD_H
#define PHILEAS_CLIPBOARD_H

#include <stddef.h>

/* Porta-retalls del sistema, amb wl-copy/wl-paste (Wayland), xclip o xsel
 * (X11). Sense escriptori o sense cap eina (p. ex. a la consola de la Pi)
 * no fa res i Phileas es queda amb el seu porta-retalls intern. */

/* Nom de l'eina detectada ("wl-clipboard", "xclip", "xsel") o NULL. */
const char *sysclip_tool(void);

/* Posa el text al porta-retalls del sistema. 0 si ha anat bé. */
int sysclip_set(const char *s, size_t n);

/* Text del porta-retalls del sistema (cal alliberar-lo), o NULL si no n'hi
 * ha, no és text o no hi ha eina. */
char *sysclip_get(size_t *n);

#endif
