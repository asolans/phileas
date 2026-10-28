#ifndef PHILEAS_CONFIG_H
#define PHILEAS_CONFIG_H

#include "editor.h"

#include <stdio.h>

#define CONFIG_SYSTEM "/etc/phileas/phileasrc"

/* Assigna (cmd = nom d'ordre) o treu (cmd = "") una drecera. Retorna -1 si
 * la tecla o l'ordre no existeixen. La fixa main.c (command.c depèn de
 * ncurses); sense, les línies "tecla." només es comproven per sobre. */
extern int (*config_bind_hook)(const char *key, const char *cmd);

/* Defineix una ordre externa (externa.NOM o externa-breu.NOM = ORDRE).
 * Retorna -1 amb el motiu a err. La fixa main.c; sense, les línies
 * «externa» només es comproven per sobre. */
extern int (*config_extern_hook)(const char *name, const char *shell, int brief,
                                 char *err, size_t errsz);

/* Una opció de la línia d'ordres, que mana sobre els fitxers. Es tornen a
 * aplicar en recarregar la configuració. */
void config_override(const char *key, const char *value);

/* Aplica una opció. Retorna 0, o -1 amb el motiu a err. */
int  config_set(Editor *e, const char *key, const char *value, char *err, size_t errsz);

/* Llegeix el fitxer del sistema (sys) i el de l'usuari (user; NULL = el
 * del directori de configuració), i després les opcions de la línia
 * d'ordres. Un fitxer que no existeix no és cap error. Retorna el nombre
 * d'errors i deixa el primer a msg (fitxer, línia i motiu). */
int  config_load(Editor *e, const char *sys, const char *user, char *msg, size_t msgsz);

/* Camí del fitxer de l'usuari. */
void config_user_path(char *out, size_t size);

/* Desa «key = value» al fitxer path (el de l'usuari): canvia les línies
 * amb aquesta clau o, si no n'hi ha cap, l'afegeix abans dels «color-»
 * (que la retoquen) o al final. La resta del fitxer no es toca. Crea el
 * fitxer i el directori si cal. Retorna -1 amb errno si no pot. */
int  config_store(const char *path, const char *key, const char *value);

/* Escriu un fitxer de configuració complet amb els valors per defecte. */
void config_template(FILE *f);

#endif
