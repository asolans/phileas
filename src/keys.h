#ifndef PHILEAS_KEYS_H
#define PHILEAS_KEYS_H

#include <stddef.h>

/* Codificació interna d'una tecla en un int:
 *   caràcter Unicode (Ctrl+lletra = 1..26, Ctrl+Espai = 0)
 *   KEY_FLAG_FN  | codi KEY_* de ncurses (fletxes, F1..., Ctrl+fletxes)
 *   KEY_FLAG_ALT | caràcter (Alt+lletra arriba com a Esc + lletra) */
#define KEY_FLAG_FN  0x40000000
#define KEY_FLAG_ALT 0x20000000

/* Converteix un nom de tecla ("ctrl-s", "alt-<", "f3", "ctrl-left"...) al
 * seu codi. Retorna -1 si el nom no és vàlid. Les tecles amb modificadors
 * ("shift-up", "ctrl-left", "ctrl-shift-end"...) sempre tenen codi, encara
 * que el terminal no les enviï (llavors queda l'alternativa amb Alt). */
int key_parse(const char *name);

/* Tecles de navegació que poden anar amb modificadors */
enum { KB_UP, KB_DOWN, KB_RIGHT, KB_LEFT, KB_HOME, KB_END, KB_PGUP, KB_PGDN,
       KB_DELETE, KB_INSERT };

/* Tecla de navegació amb modificadors. mod segueix les seqüències xterm:
 * 2 Maj, 3 Alt, 4 Maj+Alt, 5 Ctrl, 6 Ctrl+Maj, 7 Ctrl+Alt, 8 tots tres.
 * El codi és el mateix tant si el terminfo coneix la tecla com si Phileas
 * n'ha interpretat la seqüència directament. */
#define KEY_MODIFIED(nav, mod) (KEY_FLAG_FN | 0x100000 | ((nav) << 4) | (mod))

/* Nom llegible d'una tecla per a l'ajuda: "Ctrl+S", "Alt+<", "Maj+F7",
 * "Ctrl+Maj+←", "RePàg"... */
void key_describe(int key, char *out, size_t size);

/* Retornat per key_read quan el terminal ha enganxat un text */
#define KEY_PASTE    (KEY_FLAG_FN | 0xFFFFFF)

/* Temps màxim d'espera de key_read en mil·lisegons (-1 = sense límit). */
void key_set_timeout(int ms);

/* Preparen i restauren el terminal per a la lectura de tecles. */
void key_init(void);
void key_end(void);

/* Llegeix una tecla. Retorna -1 si s'acaba el temps d'espera o hi ha un error.
 * Enter arriba sempre com a '\n' i Retrocés com a KEY_FLAG_FN | KEY_BACKSPACE. */
int key_read(void);

/* Text de l'últim KEY_PASTE, en UTF-8 i amb salts '\n'. */
const char *key_paste_text(size_t *n);

#endif
