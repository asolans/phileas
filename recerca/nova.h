#ifndef RECERCA_NOVA_H
#define RECERCA_NOVA_H

#include <stddef.h>

/* Escriure al vault: fitxes noves (amb plantilla), anotacions i paperera.
 * Sense pantalla, per poder-ho provar. */

/* Nom de fitxer (sense «.md») a partir d'un títol: treu els caràcters que
 * Obsidian no admet en un nom (* " \ / < > : | ? # ^ [ ]) i els espais del
 * principi i del final. Cadena nova; pot quedar buida. */
char *nova_nom(const char *titol);

/* Carpeta de plantilles del vault: la de .obsidian/templates.json
 * («folder»), o «plantilles» si no n'hi ha. */
void nova_carpeta_plantilles(const char *root, char *out, size_t outsz);

/* Plantilles (camins relatius a root, sense ordre garantit). *out s'allibera
 * amb nova_llista_free. */
size_t nova_plantilles(const char *root, char ***out);
void   nova_llista_free(char **l, size_t n);

/* Text d'una fitxa nova. plantilla pot ser NULL (llavors, «# títol»). A la
 * plantilla se substitueixen {{title}} {{titol}} {{date}} {{data}} {{time}}
 * {{hora}}. A la capçalera hi posa id, created (data i hora) i, si anota no
 * és NULL, «anota: "[[anota]]"». ara: «AAAA-MM-DD HH:MM». Cadena nova. */
char *nova_text(const char *plantilla, const char *titol, const char *id, const char *ara,
                const char *anota);

/* Identificador nou: «pi-20261113-081530» (prefix + data i hora) */
void nova_id(const char *prefix, char *out, size_t outsz);

/* Crea carpeta/nom.md dins de root sense trepitjar res (si existeix,
 * «nom 2.md», «nom 3.md»…), creant la carpeta si cal. rel rep el camí
 * relatiu del fitxer creat. 0 si va bé. */
int nova_crea(const char *root, const char *carpeta, const char *nom, const char *text,
              char *rel, size_t relsz, char *err, size_t errsz);

/* Mou root/rel a root/.trash/ (la paperera d'Obsidian), sense trepitjar
 * res. dest rep el camí relatiu nou. 0 si va bé. */
int nova_paperera(const char *root, const char *rel, char *dest, size_t destsz, char *err, size_t errsz);

#endif
