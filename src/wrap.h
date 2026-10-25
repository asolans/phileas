#ifndef PHILEAS_WRAP_H
#define PHILEAS_WRAP_H

#include "buffer.h"

/* Calcula (si cal) les files visuals de l: talla per espais perquè cap fila
 * no passi de width columnes. Les paraules més llargues que width es tallen
 * per caràcter. Els espais que no hi caben queden penjant al final de la
 * fila. Les files de continuació de llistes i cites tenen width - hang
 * columnes. Sempre hi ha com a mínim una fila. */
void   wrap_line(Line *l, int width);

/* Fila visual que conté el byte col (cal haver cridat wrap_line). */
size_t wrap_row_of(const Line *l, size_t col);

/* Byte on acaba la fila r (exclusiu). */
size_t wrap_row_end(const Line *l, size_t r);

/* Columna on es dibuixa la fila r: 0 per a la primera, el sagnat penjant
 * per a les de continuació. */
int    wrap_row_indent(const Line *l, size_t r);

#endif
