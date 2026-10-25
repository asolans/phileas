#ifndef PHILEAS_COMMAND_H
#define PHILEAS_COMMAND_H

#include "editor.h"

/* Tota acció de l'editor és una ordre amb nom. Les dreceres de teclat, la
 * paleta (Ctrl+P) i, més endavant, el fitxer de configuració hi accedeixen
 * pel nom. */
typedef struct {
    const char *name;
    void (*fn)(Editor *e);
    const char *desc;
} Command;

const Command *cmd_find(const char *name);
const Command *cmd_for_key(int key);

/* Assigna una tecla a una ordre. Retorna -1 si la tecla no existeix en
 * aquest terminal o l'ordre és desconeguda. */
int  cmd_bind(const char *keyname, const char *cmdname);

/* Dreceres per defecte (cal que ncurses estigui inicialitzat). */
void cmd_bind_defaults(void);

#endif
