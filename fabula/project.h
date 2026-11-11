#ifndef FABULA_PROJECT_H
#define FABULA_PROJECT_H

#include <stddef.h>
#include <sys/types.h>
#include <time.h>

/* Projecte de Fàbula: una carpeta amb projecte.md a l'arrel, el manuscrit
 * (un fitxer per capítol, amb les escenes com a seccions ###) i grups de
 * fitxes (personatges, llocs, recerca, notes…). Tot són fitxers Markdown. */

typedef enum {
    FK_ROOT,      /* arrel invisible: els fills són els grups */
    FK_GROUP,     /* carpeta: Manuscrit, Personatges… */
    FK_CHAPTER,   /* fitxer del manuscrit */
    FK_SCENE,     /* secció ### d'un capítol */
    FK_CARD,      /* fitxa */
} FKind;

enum { FK_OWN_POV = 1, FK_OWN_LLOC = 2, FK_OWN_DATA = 4, FK_OWN_PERS = 8, FK_OWN_SINOPSI = 16 };

typedef struct FNode FNode;
struct FNode {
    FKind   kind;
    char   *title;
    char   *name;       /* nom del fitxer o carpeta (sense camí); NULL a les escenes */
    char   *path;       /* camí absolut del fitxer o carpeta; NULL a les escenes */
    char   *estat;      /* estat (capítols i fitxes); sempre n'hi ha si el projecte té estats */
    int     estat_set;  /* l'estat és escrit al fitxer, no és el per defecte */
    char   *sinopsi;    /* pot ser buit */
    /* Metadades (capítols i fitxes: YAML; escenes: comentari sota la capçalera,
     * i si no en tenen en prenen les del capítol). Mai NULL. */
    char   *pov;
    char   *lloc;
    char   *data;       /* AAAA, AAAA-MM o AAAA-MM-DD */
    char   *pers;       /* personatges, separats per comes */
    unsigned own;       /* escenes: quins camps són propis (FK_OWN_*), la resta, heretats */
    size_t  words;      /* paraules; als grups, el total dels fills */
    size_t  line;       /* escenes: línia del buffer de la capçalera */
    FNode  *parent;
    FNode **kids;
    size_t  nkids;
    int     open;       /* desplegat a la vista */
    long long mtime;    /* fitxers: mtime en ns quan es va llegir; per saber si ha canviat */
    off_t   fsize;
    ino_t   ino;        /* Phileas desa amb rename: un fitxer nou té un altre inode */
};

typedef struct {
    char   *root;       /* camí absolut de la carpeta */
    char   *titol;
    char   *autor;
    char   *idioma;
    long    objectiu;   /* paraules del manuscrit; 0 = sense */
    char   *data_limit; /* AAAA-MM-DD o buit */
    char  **estats;
    size_t  nestats;
    FNode  *top;
    FNode  *manuscrit;  /* el grup «manuscrit», o NULL */
} Project;

/* Obre i escaneja la carpeta. Retorna 0, o -1 amb el motiu a err. */
int  project_open(Project *p, const char *dir, char *err, size_t errsz);
void project_free(Project *p);

/* Torna a escanejar la carpeta (després d'editar): només es llegeixen els
 * fitxers que han canviat o són nous; la resta es reaprofita. Conserva què
 * és desplegat. Si falla, el projecte queda com estava. Tots els FNode
 * anteriors queden invàlids. */
int  project_refresh(Project *p, char *err, size_t errsz);

/* Crea la carpeta d'un projecte nou dins de parent (CARPETA/títol-en-slug):
 * projecte.md i els grups manuscrit, personatges, llocs, recerca i notes.
 * Si ja existeix i no és buida, no fa res. path_out és la carpeta creada. */
int  project_create(const char *parent, const char *title, char *path_out, size_t psz,
                    char *err, size_t esz);

/* Paraules del manuscrit */
size_t project_words(const Project *p);

/* Dies naturals des de now fins a la data límit (inclosa), o -1 si no n'hi
 * ha o és passada. */
long   project_days_left(const Project *p, time_t now);

/* Paraules diàries que calen per arribar a l'objectiu, o -1 si no es pot
 * calcular (sense objectiu ni data) i 0 si ja s'ha assolit. */
long   project_daily_needed(const Project *p, time_t now);

/* Canvia l'estat d'un capítol o fitxa i el desa al fitxer (escriptura
 * atòmica; només es reescriu la línia `estat`). 0, o -1 amb errno. */
int    project_set_estat(Project *p, FNode *n, const char *estat);

/* ---- Operacions sobre els fitxers ----
 * Cap no modifica l'arbre en memòria: després cal project_refresh i, si es
 * vol, buscar el fitxer pel camí (path_out). Mai no sobreescriuen ni
 * esborren res. Els errors, en català a err. */

/* Fitxer nou al grup (NULL = el manuscrit, que es crea si cal): un capítol
 * (NN-títol.md, el número següent en passos de 10) o una fitxa (títol.md),
 * amb la plantilla de plantilles/ del projecte o la de fàbrica. */
int project_new_item(Project *p, FNode *group, const char *title, char *path_out, size_t psz,
                     char *err, size_t esz);

/* Títol nou d'un capítol o fitxa: posa `titol:` a la capçalera (el text no es
 * toca) i canvia el nom del fitxer, sense el número de capítol. */
int project_rename(Project *p, FNode *n, const char *title, char *path_out, size_t psz,
                   char *err, size_t esz);

/* Mou el fitxer a .paperera/ (amb data al nom). */
int project_trash(Project *p, FNode *n, char *err, size_t esz);

/* Capítol amunt (dir < 0) o avall (dir > 0): intercanvia els números dels
 * noms amb el veí, o renumera tots en passos de 10 si no es pot. */
int project_move(Project *p, FNode *n, int dir, char *path_out, size_t psz, char *err, size_t esz);

/* ---- Instantànies (.versions/GRUP/nom.AAAAMMDD-HHMMSS.md) ----
 * Còpies d'un capítol o fitxa. Mai s'esborra res: les que passen de
 * FK_VERSIONS_MAX van a .paperera/. now és l'hora (per poder-ho provar). */
#define FK_VERSIONS_MAX 20
#define FK_SNAPSHOT_GAP 3600   /* segons entre dues instantànies automàtiques */

typedef struct {
    char   stamp[16];   /* AAAAMMDD-HHMMSS */
    time_t when;
    off_t  size;
    size_t words;       /* paraules de prosa de la versió */
    char  *path;
} FVersion;

/* Retorna 0 si n'ha fet una, 1 si no calia (idèntica a la darrera, o, sense
 * force, la darrera és de fa menys d'una hora) i -1 amb err si falla. */
int    project_snapshot(Project *p, FNode *n, int force, time_t now, char *err, size_t esz);

/* Les versions d'un fitxer, de la més nova a la més antiga. Retorna el
 * nombre (0 si no n'hi ha); *v s'ha d'alliberar amb project_versions_free. */
size_t project_versions(const Project *p, const FNode *n, FVersion **v);
void   project_versions_free(FVersion *v, size_t n);

/* Posa la versió com a fitxer actual; abans en fa una instantània de
 * l'estat actual (així es pot tornar enrere). */
int    project_restore(Project *p, FNode *n, const FVersion *v, time_t now, char *err, size_t esz);

/* Canvia un camp de metadades i el desa: key és pov, lloc, data, personatges,
 * sinopsi o estat; value buit el treu. En un capítol o fitxa és la capçalera
 * YAML; en una escena, el comentari `<!-- pov: Anna; lloc: Mas -->` just sota
 * la capçalera ### (es crea si no hi és). L'escena es busca per l'ordre i es
 * comprova pel títol: si el fitxer ha canviat, no es toca res. Escriptura
 * atòmica; l'arbre en memòria no canvia (cal project_refresh). */
int    project_set_meta(Project *p, FNode *n, const char *key, const char *value,
                        char *err, size_t esz);

/* Estat següent de la llista del projecte (amb tornada al primer) */
const char *project_next_estat(const Project *p, const char *estat);

/* Utilitats d'ordre i noms (provades) */
int    fk_match(const char *text, const char *needle);
int    fk_node_match(const FNode *n, const char *needle);   /* títol, sinopsi i metadades */  /* conté, sense majúscules ni accents */
void   fk_fmt_num(size_t n, char *out, size_t outsz);  /* 42310 -> «42.310» */
int    fk_name_cmp(const char *a, const char *b);   /* comparació natural */
char  *fk_slug(const char *title);                  /* nom de fitxer ASCII, sense extensió */

#endif
