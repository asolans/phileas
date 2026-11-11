#ifndef FABULA_FLAT_H
#define FABULA_FLAT_H

#include "project.h"

/* L'arbre aplanat, en les files que es veuen: els grups, i els fills dels
 * nodes desplegats. Amb un filtre, només els elements que el compleixen (pel
 * títol o la sinopsi) i els seus pares, desplegats. */
typedef struct {
    FNode *node;
    int    depth;
} FRow;

typedef struct {
    FRow  *rows;
    size_t n, cap;
} Flat;

void flat_build(Flat *f, const Project *p, const char *filter);
void flat_free(Flat *f);

/* Índex de la fila d'aquest node, o -1 */
long flat_find(const Flat *f, const FNode *n);

#endif
