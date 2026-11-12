#ifndef PHILEAS_SPELL_H
#define PHILEAS_SPELL_H

#include <stddef.h>

/* Correcció ortogràfica amb hunspell. La biblioteca es carrega en temps
 * d'execució (dlopen): si no hi és, Phileas funciona igual, sense ortografia. */

/* Carrega el diccionari lang ("ca", "es_ES"...), o el de la configuració
 * regional si és NULL (ca_ES, després ca, i si no, "ca"). Si lang és només
 * un idioma ("es"), també es prova "es_ES". Retorna 0, o -1 amb el motiu a
 * spell_error(). El diccionari anterior es manté si falla. */
int  spell_open(const char *lang);

/* Es mantenen carregats fins a dos diccionaris: tornar a obrir-ne un que
 * ja hi és és instantani (F8). En carregar-ne un tercer, es descarta el
 * que fa més temps que no es fa servir. spell_loaded diu si lang ja hi és. */
int  spell_loaded(const char *lang);

/* Descarrega tots els diccionaris. */
void spell_close(void);
const char *spell_lang(void);      /* NULL si no hi ha diccionari */

/* Codi d'idioma d'un nom de diccionari o de configuració regional
 * ("ca_ES-valencia" -> "ca", "es_ES.UTF-8" -> "es"). Amb NULL, el de la
 * configuració regional, o "ca" si no n'hi ha. */
void spell_code(const char *name, char *out, size_t size);
const char *spell_error(void);

/* 1 si la paraula s[0..n) és correcta (o no hi ha diccionari). */
int  spell_check(const char *s, size_t n);

/* Suggeriments per a la paraula, a list (cal alliberar-los amb
 * spell_free_list). Retorna el nombre de suggeriments. */
int  spell_suggest(const char *s, size_t n, char ***list);
void spell_free_list(char **list, int n);

/* Afegeix la paraula al diccionari personal de l'idioma actual (es desa a
 * ~/.config/phileas/diccionari-ca.txt, -es.txt...) o la ignora fins que es
 * tanqui Phileas.
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

/* Diccionari de projecte: el fitxer .diccionari.txt de la carpeta del document
 * (o d'una de superior, fins a 6 nivells), una paraula per línia, vàlid per
 * a tots els idiomes. spell_project_set el busca per al document (NULL: cap) i
 * en carrega les paraules; es crida en obrir un document. spell_add_project hi
 * afegeix una paraula; si no n'hi ha cap, el crea a l'arrel del projecte de
 * Fàbula (la carpeta amb projecte.md) o a la carpeta del document. */
void spell_project_set(const char *docpath);
const char *spell_project_file(void);       /* NULL si no n'hi ha */
int  spell_add_project(const char *s, size_t n, const char *docpath);

#endif
