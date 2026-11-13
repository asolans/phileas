#ifndef COMU_UI_H
#define COMU_UI_H

#include "fterm.h"
#include "rc.h"

#include "keys.h"

#include <stddef.h>

/* Tecla de funció de ncurses tal com la retorna key_read() */
#define K_FN(k) (KEY_FLAG_FN | (k))

/* Peces de pantalla comunes de Fàbula i Recerca (sobre fterm). */

/* Inicia el terminal amb el tema, la paleta i els colors de l (el que
 * s'ha llegit del phileasrc amb cm_look_key) */
void cm_look_start(const CmLook *l);

/* Amplada en columnes d'un text UTF-8 */
int  cm_width(const char *s);
/* Escriu s a (y, x) en com a molt maxw columnes; si no hi cap, l'acaba amb
 * «…». Retorna les columnes escrites. */
int  cm_put(int y, int x, const char *s, int maxw, attr_t a);
/* w espais a (y, x) amb l'atribut a */
void cm_blank(int y, int x, int w, attr_t a);

/* Funció que torna a pintar la pantalla de sota (d'un diàleg); pot ser NULL */
typedef void (*cm_paint_fn)(void *ud);

/* Línia d'entrada a la barra d'estat. Retorna 1 si s'accepta (Retorn) i 0 si
 * es cancel·la (Esc); buf conserva el text. */
int  cm_prompt(const char *label, const char *initial, char *buf, size_t sz,
               cm_paint_fn paint, void *ud);
/* Pregunta «(s/n)» a la barra d'estat: 1 si la resposta és s/S (o y/Y) */
int  cm_confirm(const char *question, cm_paint_fn paint, void *ud);

/* Menú d'Esc: una llista d'opcions agrupades. Un element amb key 0 és la
 * capçalera d'un grup. Retorna la tecla de l'opció triada, o -1 (Esc, F10). */
typedef struct {
    const char *label, *hint;
    int         key;
} CmMenuItem;

int  cm_menu(const CmMenuItem *items, int n, cm_paint_fn paint, void *ud);

/* Pantalla d'ajuda: la primera línia és el títol. Si no hi cap, ↑ ↓ RePàg
 * AvPàg la desplacen; qualsevol altra tecla la tanca. */
void cm_help(const char *const *lines, size_t n);

#endif
