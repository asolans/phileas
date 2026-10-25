#ifndef PHILEAS_RENDER_H
#define PHILEAS_RENDER_H

#include "editor.h"

void ui_init(void);
void ui_end(void);

/* Recalcula la disposició i dibuixa la pantalla (sense refresh()). */
void render(Editor *e);

/* Demana un text a la línia inferior. buf conté el valor inicial i hi
 * queda la resposta en UTF-8. Retorna 1 amb Enter i 0 amb Esc. Si changed
 * no és NULL, es crida cada vegada que el text canvia (cerca incremental). */
int  prompt(Editor *e, const char *label, char *buf, size_t size,
            void (*changed)(Editor *e, const char *text));

/* Pregunta amb resposta d'una tecla d'entre choices (minúscules). Retorna
 * la tecla triada, o 0 amb Esc. */
int  prompt_choice(Editor *e, const char *question, const char *choices);

/* Pregunta de sí o no. Retorna 1 si la resposta és «s». */
int  prompt_yesno(Editor *e, const char *question);

#endif
