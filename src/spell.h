#ifndef PHILEAS_SPELL_H
#define PHILEAS_SPELL_H

#include <stddef.h>

/* Correcció ortogràfica amb hunspell. La biblioteca es carrega en temps
 * d'execució (dlopen): si no hi és, Phileas funciona igual, sense ortografia. */

/* Carrega el diccionari lang ("ca", "es_ES"...), o el de la configuració
 * regional si és NULL (ca_ES, després ca, i si no, "ca"). Retorna 0, o -1
 * amb el motiu a spell_error(). El diccionari anterior es manté si falla. */
int  spell_open(const char *lang);
void spell_close(void);
const char *spell_lang(void);      /* NULL si no hi ha diccionari */
const char *spell_error(void);

/* 1 si la paraula s[0..n) és correcta (o no hi ha diccionari). */
int  spell_check(const char *s, size_t n);

/* Suggeriments per a la paraula, a list (cal alliberar-los amb
 * spell_free_list). Retorna el nombre de suggeriments. */
int  spell_suggest(const char *s, size_t n, char ***list);
void spell_free_list(char **list, int n);

/* Afegeix la paraula al diccionari personal (es desa a
 * ~/.config/phileas/diccionari.txt) o la ignora fins que es tanqui Phileas.
 * spell_add retorna -1 amb errno si no s'ha pogut desar. */
int  spell_add(const char *s, size_t n);
void spell_ignore(const char *s, size_t n);

/* Paraula següent de s[from..len): lletres, amb ' ’ · i - entre lletres.
 * Retorna 0 si no n'hi ha. Les paraules amb xifres i les que formen part
 * d'una adreça (http://, www., correu, nom.de.fitxer) es descarten. */
int  spell_next_word(const char *s, size_t len, size_t from, size_t *a, size_t *b);

/* Primer error ortogràfic de s[from..len) segons els estils de md_style
 * (no es revisen el codi ni els marcadors). Retorna 0 si no n'hi ha. */
int  spell_next_error(const char *s, size_t len, const unsigned char *styles,
                      size_t from, size_t *a, size_t *b);

#endif
