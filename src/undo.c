#include "undo.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>

/* Límit de memòria de l'historial: en passar-lo s'obliden els grups més
 * antics. 8 MB és molta escriptura i cap en una Raspberry Pi petita. */
#define UNDO_MAX_BYTES (8u << 20)

static void drop_redo(Undo *u)
{
    for (size_t i = u->pos; i < u->n; i++) {
        u->bytes -= u->recs[i].len;
        free(u->recs[i].text);
    }
    u->n = u->pos;
}

/* Oblida el grup més antic, mentre no sigui l'únic que queda. */
static void drop_oldest(Undo *u)
{
    size_t k = 0;

    while (k < u->n && u->recs[k].group == u->recs[0].group)
        k++;
    if (k == u->n || k > u->pos)
        return;
    for (size_t i = 0; i < k; i++) {
        u->bytes -= u->recs[i].len;
        free(u->recs[i].text);
    }
    u->base_seq = u->recs[k - 1].seq;
    memmove(u->recs, u->recs + k, (u->n - k) * sizeof *u->recs);
    u->n -= k;
    u->pos -= k;
}

void undo_push(Undo *u, int type, UndoPos a, UndoPos z, char *text, size_t len)
{
    UndoRec *r;

    drop_redo(u);
    if (u->n == u->cap) {
        u->cap = u->cap ? u->cap * 2 : 256;
        u->recs = xrealloc(u->recs, u->cap * sizeof *u->recs);
    }
    r = &u->recs[u->n++];
    r->type = type;
    r->a = a;
    r->z = z;
    r->text = text;
    r->len = len;
    r->group = u->group;
    r->seq = ++u->seq;
    u->pos = u->n;
    u->bytes += len;
    while (u->bytes > UNDO_MAX_BYTES) {
        size_t before = u->n;
        drop_oldest(u);
        if (u->n == before)
            break;
    }
}

void undo_mark(Undo *u)
{
    u->group++;
}

unsigned long undo_state(const Undo *u)
{
    return u->pos > 0 ? u->recs[u->pos - 1].seq : u->base_seq;
}

void undo_free(Undo *u)
{
    for (size_t i = 0; i < u->n; i++)
        free(u->recs[i].text);
    free(u->recs);
    memset(u, 0, sizeof *u);
}
