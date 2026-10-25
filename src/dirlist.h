#ifndef PHILEAS_DIRLIST_H
#define PHILEAS_DIRLIST_H

#include <stddef.h>
#include <sys/types.h>
#include <time.h>

/* Contingut d'un directori per al navegador de fitxers. */
typedef struct {
    char   *name;
    int     is_dir;      /* també els enllaços simbòlics a directoris */
    off_t   size;
    time_t  mtime;
} DirEntry;

typedef struct {
    DirEntry *items;
    size_t    n, cap;
} DirList;

/* Llegeix el directori dir: primer els subdirectoris i després els fitxers,
 * cadascun per ordre alfabètic de la configuració regional. Sense "." ni
 * "..", i sense els noms que comencen per '.' si hidden és 0. Retorna 0, o
 * -1 amb errno (la llista queda buida). */
int  dirlist_read(DirList *l, const char *dir, int hidden);
void dirlist_free(DirList *l);

/* 1 si name conté filter, sense distingir majúscules (filtre buit: sí). */
int  dirlist_match(const char *name, const char *filter);

/* Directori pare de path (absolut i normalitzat), a out. "/" és el seu propi pare. */
void path_parent(const char *path, char *out, size_t outsz);

/* Uneix dir i name a out. */
void path_join(const char *dir, const char *name, char *out, size_t outsz);

/* Mida en format curt: "512 B", "12 KB", "3,4 MB". */
void format_size(off_t size, char *out, size_t outsz);

#endif
