#ifndef FABULA_DIARY_H
#define FABULA_DIARY_H

#include <stddef.h>
#include <time.h>

/* Diari d'escriptura: .fabula/diari.csv, una línia per dia amb feina:
 *   AAAA-MM-DD,paraules-a-l'inici,paraules-al-final
 * L'inici és el final del dia anterior registrat (així compta el que s'ha
 * escrit fora de Fàbula); el primer dia, el total d'aquell moment. */

typedef struct {
    long day;       /* dies des de 1970-01-01 (data local) */
    long start, end;
} DiaryDay;

typedef struct {
    DiaryDay *days; /* ordenats per dia */
    size_t    n;
} Diary;

int  diary_load(const char *root, Diary *d);            /* sense fitxer: buit, retorna 0 */
void diary_free(Diary *d);
/* Anota el total del manuscrit per a avui (crea o actualitza). 0, o -1 amb errno. */
int  diary_record(const char *root, long words, time_t now);

long diary_day_of(time_t t);                            /* dia local */
void diary_day_str(long day, char *out, size_t sz);     /* «AAAA-MM-DD» */
long diary_parse_day(const char *s);                    /* «AAAA-MM-DD» -> dia, o -1 */

typedef struct {
    long today;         /* paraules netes escrites avui */
    long week[7];       /* els 7 dies fins avui, week[6] = avui (net, pot ser negatiu) */
    long week_total;
    long avg;           /* mitjana diària (només el que s'ha escrit) dels darrers 30 dies */
    long best;          /* el millor dia de tot el diari */
    int  streak;        /* dies seguits escrivint, comptant fins avui (o fins ahir si avui encara res) */
    int  active_days;   /* dies amb feina al diari */
    long remaining;     /* per a l'objectiu, o -1 si no n'hi ha; 0 si assolit */
    long days_to_goal;  /* a la mitjana actual; -1 si no es pot calcular */
    long finish_day;    /* dia previst d'acabar, o -1 */
    long limit_day;     /* dia límit, o -1 */
} DiaryStats;

void diary_stats(const Diary *d, long total, long objectiu, const char *data_limit, time_t now,
                 DiaryStats *s);

/* L'informe en línies de text (cada línia i el vector, amb free). */
char **diary_report(const Diary *d, long total, long objectiu, const char *data_limit, time_t now,
                    size_t *nlines);

#endif
