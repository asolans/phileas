#ifndef PHILEAS_RENDER_H
#define PHILEAS_RENDER_H

#include "editor.h"

void ui_init(void);

/* Aplica els colors de e->palette (en arrencar i en recarregar la configuració). */
void render_colors(Editor *e);
void ui_end(void);

/* Deixa el terminal en mode normal per executar un altre programa, i hi
 * torna. ui_resume() redibuixa la pantalla sencera i adopta la mida
 * actual del terminal, que el programa pot haver canviat. */
void ui_suspend(void);
void ui_resume(void);

/* Si la mida del terminal ha canviat (p. ex. una altra lletra de la
 * consola), l'adopta sense esperar el senyal. */
void ui_sync_size(void);

/* Recalcula la disposició i dibuixa la pantalla (sense refresh()). */
void render(Editor *e);

/* Demana un text a la línia inferior. buf conté el valor inicial i hi
 * queda la resposta en UTF-8. Retorna 1 amb Enter i 0 amb Esc. Si changed
 * no és NULL, es crida cada vegada que el text canvia (cerca incremental). */
int  prompt(Editor *e, const char *label, char *buf, size_t size,
            void (*changed)(Editor *e, const char *text));

/* Com prompt(), però Tab crida complete, que pot allargar buf i deixar a
 * hint un text (p. ex. les opcions) que es mostra atenuat darrere. */
int  prompt_complete(Editor *e, const char *label, char *buf, size_t size,
                     void (*complete)(Editor *e, char *buf, size_t size,
                                      char *hint, size_t hintsz));

/* Pregunta amb resposta d'una tecla d'entre choices (minúscules). Retorna
 * la tecla triada, o 0 amb Esc. */
int  prompt_choice(Editor *e, const char *question, const char *choices);

/* Pregunta de sí o no. Retorna 1 si la resposta és «s». */
int  prompt_yesno(Editor *e, const char *question);

#endif
