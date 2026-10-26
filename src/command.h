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
    int act;            /* ACT_* per agrupar el desfer; 0 = acció pròpia */
} Command;

const Command *cmd_find(const char *name);

/* Executa una ordre. args: el que s'ha escrit a la paleta després del nom
 * (NULL o "" si no res); només les ordres externes n'admeten. */
void cmd_run(Editor *e, const Command *c, const char *args);

/* Ordres externes (phileasrc: externa.NOM i externa-breu.NOM). Una
 * definició nova amb el mateix nom substitueix l'anterior. */
int  cmd_config_extern(const char *name, const char *shell, int brief, char *err, size_t errsz);
void cmd_extern_clear(void);

/* Per a l'ajuda: totes les ordres, i les tecles assignades a cadascuna
 * (com a molt max; retorna quantes n'hi ha). */
size_t cmd_count(void);
const Command *cmd_at(size_t i);
int  cmd_keys(const Command *c, int *keys, int max);
const Command *cmd_for_key(int key);

/* Assigna una tecla a una ordre. Retorna -1 si la tecla no existeix en
 * aquest terminal o l'ordre és desconeguda. */
int  cmd_bind(const char *keyname, const char *cmdname);

/* Treu la drecera d'una tecla. Retorna -1 si el nom de tecla no existeix. */
int  cmd_unbind(const char *keyname);

/* Deixa només les dreceres per defecte. */
void cmd_bind_defaults(void);

/* Per a la configuració: "tecla.NOM = ordre", o buit per treure-la. */
int  cmd_config_bind(const char *keyname, const char *cmdname);

#endif
