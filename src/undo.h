#ifndef PHILEAS_UNDO_H
#define PHILEAS_UNDO_H

#include <stddef.h>

/* Historial de desfer/refer: una llista d'operacions elementals (inserir o
 * esborrar un text en una posició) agrupades. Desfer recula un grup sencer
 * (una paraula escrita, una substitució de tot el document...). */

enum { UNDO_INSERT, UNDO_DELETE };

typedef struct {
    size_t line, col;
} UndoPos;

typedef struct {
    int     type;
    UndoPos a, z;           /* tram afectat: inserit (a..z) o esborrat */
    char   *text;
    size_t  len;
    unsigned long group;
    unsigned long seq;      /* identifica l'estat del document després d'aplicar-la */
} UndoRec;

typedef struct {
    UndoRec *recs;
    size_t   n, cap;
    size_t   pos;           /* operacions aplicades: [0, pos); per refer: [pos, n) */
    size_t   bytes;         /* memòria de text ocupada */
    unsigned long group;    /* grup de les operacions noves */
    unsigned long seq;      /* últim número assignat */
    unsigned long base_seq; /* estat quan pos == 0 */
    unsigned long saved_seq;/* estat de l'últim desat */
    int      applying;      /* s'està desfent o refent: no s'enregistra */
} Undo;

/* Afegeix una operació (es queda text) i descarta el que es podia refer. */
void undo_push(Undo *u, int type, UndoPos a, UndoPos z, char *text, size_t len);

/* Les operacions següents formaran un grup nou. */
void undo_mark(Undo *u);

/* Estat actual del document (per saber si coincideix amb l'últim desat). */
unsigned long undo_state(const Undo *u);

void undo_free(Undo *u);

#endif
