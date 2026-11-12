#ifndef PHILEAS_HELP_H
#define PHILEAS_HELP_H

#include "editor.h"
#include "stats.h"

/* Ajuda a pantalla completa (F1): totes les ordres agrupades per temes, amb
 * les tecles que hi ha assignades ara mateix. Es pot filtrar escrivint. */
void help_show(Editor *e);

/* Quant a: la versió de Phileas i, si n'hi ha, la del sistema
 * (/etc/byok-version a la imatge BYOK). Es tanca amb qualsevol tecla. */
void about_show(Editor *e);

/* Estadístiques (paraules, caràcters, frases, pàgines, lectura) en un
 * quadre; qualsevol tecla el tanca. */
void stats_show(Editor *e, const DocStats *st, int selection);

#endif
