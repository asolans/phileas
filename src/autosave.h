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

/* Desat d'emergència (en rebre SIGHUP o SIGTERM): si hi ha canvis, desa el
 * document sobre el seu fitxer. Si no té nom o no s'hi pot desar, el desa al
 * directori de configuració com a rescat-AAAAMMDD-HHMMSS.md (o .txt), i si
 * tampoc, a $HOME. Retorna 0 si no hi havia canvis (path buit) o si s'ha
 * desat (path = on), i -1 si no s'ha pogut desar enlloc. */
int  autosave_emergency(Editor *e, char *path, size_t size);

#endif
