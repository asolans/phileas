#ifndef PHILEAS_AUTOSAVE_H
#define PHILEAS_AUTOSAVE_H

#include "editor.h"

/* Rellotge monòton en segons. */
double now_seconds(void);

/* Desa el document sobre el mateix fitxer si hi ha canvis i fa e->autosave
 * segons que no es prem cap tecla, o si fa e->autosave_max segons que hi ha
 * canvis sense desar. */
void autosave_tick(Editor *e, double now);

/* Mil·lisegons fins al pròxim autodesat (-1 si no n'hi ha cap pendent). */
int  autosave_wait_ms(const Editor *e, double now);

#endif
