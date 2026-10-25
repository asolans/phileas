#ifndef PHILEAS_KEYS_H
#define PHILEAS_KEYS_H

/* Codificació interna d'una tecla en un int:
 *   caràcter Unicode (Ctrl+lletra = 1..26, Ctrl+Espai = 0)
 *   KEY_FLAG_FN  | codi KEY_* de ncurses (fletxes, F1..., Ctrl+fletxes)
 *   KEY_FLAG_ALT | caràcter (Alt+lletra arriba com a Esc + lletra) */
#define KEY_FLAG_FN  0x40000000
#define KEY_FLAG_ALT 0x20000000

/* Converteix un nom de tecla ("ctrl-s", "alt-<", "f3", "ctrl-left"...) al
 * seu codi. Retorna -1 si el nom no és vàlid o si el terminal actual no té
 * aquesta tecla (p. ex. Ctrl+fletxes a la consola de Linux). Cal que ncurses
 * estigui inicialitzat. */
int key_parse(const char *name);

/* Llegeix una tecla (bloquejant). Retorna -1 si hi ha hagut un error. */
int key_read(void);

#endif
