#ifndef RECERCA_VAULT_H
#define RECERCA_VAULT_H

#include <stddef.h>

#include "sqlite3.h"

/* Un vault de recerca: una carpeta de notes Markdown (un vault d'Obsidian)
 * i el seu índex SQLite a VAULT/.recerca/index.db. Els fitxers són la
 * veritat; l'índex és una memòria cau que es pot esborrar i refer.
 *
 * Taules (vegeu ESQUEMA a vault.c):
 *   fitxes     una per nota (.md): camí, nom, tipus, data, id, paraules…
 *   propietats totes les propietats YAML (clau, valor, ordre)
 *   etiquetes  de la capçalera (linia 0) i del text
 *   alies      `aliases` (i `alies`), en minúscules
 *   enllacos   [[ ]] i enllaços Markdown interns, del text i de les
 *              propietats (relacio = la clau), resolts a `a`
 *   blocs      identificadors ^bloc
 *   text       FTS5 sense contingut (detail=none): nom, propietats i cos */

#define VAULT_DIR    ".recerca"
#define VAULT_DB     ".recerca/index.db"
#define VAULT_SCHEMA "1"

typedef struct Vault Vault;

typedef struct {
    size_t noves, canviades, esborrades, iguals;
    double segons;
} VaultSync;

/* Progrés de la sincronització: fet de total fitxers per indexar */
typedef void (*vault_progress_fn)(size_t fet, size_t total, void *ud);

/* Obre el vault de l'arrel root (crea .recerca/ i l'índex si cal; si
 * l'esquema és d'una altra versió, el buida). NULL i err si no es pot. */
Vault *vault_open(const char *root, char *err, size_t errsz);
void   vault_close(Vault *v);

/* Posa l'índex al dia: indexa les notes noves o canviades (mtime, mida o
 * inode) i treu les esborrades, en una sola transacció; després torna a
 * resoldre els enllaços. Amb refes=1 ho buida tot abans. 0 si va bé. */
int vault_sync(Vault *v, int refes, vault_progress_fn fn, void *ud, VaultSync *out,
               char *err, size_t errsz);

/* Reindexa una sola nota (camí relatiu a l'arrel), p. ex. en tornar de
 * Phileas. Si ja no existeix, la treu. Torna a resoldre els enllaços. */
int vault_reindex(Vault *v, const char *rel, char *err, size_t errsz);

sqlite3    *vault_db(const Vault *v);
const char *vault_root(const Vault *v);

/* 1.234.567 (amb punts de milers) a out */
void vault_fmt_num(char *out, size_t outsz, long long n);

#endif
