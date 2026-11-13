#ifndef COMU_RC_H
#define COMU_RC_H

#include <stddef.h>

/* Lectura del phileasrc (el de Phileas: «clau = valor», # comentaris).
 * Crida fn per a cada clau del fitxer del sistema (/etc/phileas/phileasrc) i
 * després del de l'usuari (~/.config/phileas/phileasrc), de manera que
 * l'usuari mana. */
typedef void (*cm_rc_fn)(const char *key, const char *value, void *ud);

void cm_rc_read(cm_rc_fn fn, void *ud);

/* El valor de key als dos fitxers (val el darrer), amb «~» expandit, a out.
 * Retorna 1 si hi és, 0 si no. És el que escriu «phileas --valor CLAU». */
int  cm_rc_get(const char *key, char *out, size_t size);

/* Claus del phileasrc que no són de Phileas: les comunes a les tres eines i
 * les de Recerca. Phileas les accepta, les conserva i les escriu a la
 * plantilla de desar-configuracio, encara que no les faci servir. */
enum {
    CM_K_EDITOR,            /* editor de Fàbula i Recerca (per defecte, phileas) */
    CM_K_TEXTOS,            /* carpeta dels textos */
    CM_K_PROJECTES,         /* carpeta dels projectes de Fàbula */
    CM_K_RECERQUES,         /* carpeta dels vaults de Recerca */
    CM_K_RECERCA_VAULT,     /* vault per defecte de Recerca */
    CM_K_RECERCA_REPLICA,   /* el vault és una rèplica (només s'hi escriu a l'entrada) */
    CM_K_RECERCA_ENTRADA,   /* carpeta d'entrada de la rèplica */
    CM_NSHARED
};
extern const char *const cm_shared_keys[CM_NSHARED];
/* L'índex de key a cm_shared_keys, o -1 */
int  cm_shared_key(const char *key);

#include "palette.h"

/* Claus «color-…» del phileasrc i el color de la paleta que canvien (la
 * taula de Phileas, de Fàbula i de Recerca) */
typedef struct {
    const char *key;
    int         pal;
} CmColorKey;

extern const CmColorKey cm_color_keys[];
extern const size_t     cm_ncolor_keys;

/* Colors que canvien els del tema, com a Phileas («color-barra = blau»…).
 * over[] comença amb CM_NO_COLOR (cap canvi). Si key és una d'aquestes
 * claus, hi posa el color i retorna 1. Una línia «tema» posterior els
 * esborra (cm_rc_color_reset), com a Phileas. */
#define CM_NO_COLOR (-2)
int  cm_rc_color(const char *key, const char *value, short over[PAL_N]);
void cm_rc_color_reset(short over[PAL_N]);
/* pal = la del tema amb els canvis de over */
void cm_rc_apply(short pal[PAL_N], const short over[PAL_N]);

/* L'aspecte comú (tema, paleta de la consola i colors), com a Phileas.
 * cm_look_key retorna 1 si la clau és d'aquestes. ui.h el posa en marxa. */
typedef struct {
    char  tema[64];
    int   console;
    short over[PAL_N];
} CmLook;

void cm_look_init(CmLook *l);
int  cm_look_key(CmLook *l, const char *key, const char *value);

/* «si», «sí», «yes», «true», «1» → 1; la resta → 0 */
int  cm_rc_bool(const char *value);
/* Un camí del phileasrc, amb «~» en lloc del HOME */
void cm_rc_path(const char *value, char *out, size_t size);

#endif
