#include "diary.h"

#include <errno.h>
#include <stdarg.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* algorisme de Howard Hinnant: dies des de 1970-01-01 per a una data civil */
static long days_from_civil(long y, long m, long d)
{
    long era, yoe, doy, doe;

    y -= m <= 2;
    era = (y >= 0 ? y : y - 399) / 400;
    yoe = y - era * 400;
    doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

static void civil_from_days(long z, long *y, long *m, long *d)
{
    long era, doe, yoe, doy, mp;

    z += 719468;
    era = (z >= 0 ? z : z - 146096) / 146097;
    doe = z - era * 146097;
    yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    *y = yoe + era * 400;
    doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    mp = (5 * doy + 2) / 153;
    *d = doy - (153 * mp + 2) / 5 + 1;
    *m = mp + (mp < 10 ? 3 : -9);
    *y += *m <= 2;
}

long diary_civil_days(long y, long m, long d)
{
    return days_from_civil(y, m, d);
}

long diary_day_of(time_t t)
{
    struct tm tm;

    localtime_r(&t, &tm);
    return days_from_civil(tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
}

void diary_day_str(long day, char *out, size_t sz)
{
    long y, m, d;

    civil_from_days(day, &y, &m, &d);
    snprintf(out, sz, "%04ld-%02ld-%02ld", y, m, d);
}

long diary_parse_day(const char *s)
{
    int y, m, d, n = 0;

    if (!s || sscanf(s, "%4d-%2d-%2d%n", &y, &m, &d, &n) != 3 || n != 10 || m < 1 || m > 12 ||
        d < 1 || d > 31)
        return -1;
    return days_from_civil(y, m, d);
}

static char *diary_path(const char *root, int dir)
{
    size_t n = strlen(root) + 32;
    char *p = malloc(n);

    if (p)
        snprintf(p, n, dir ? "%s/.fabula" : "%s/.fabula/diari.csv", root);
    return p;
}

static int cmp_day(const void *a, const void *b)
{
    long x = ((const DiaryDay *)a)->day, y = ((const DiaryDay *)b)->day;

    return x < y ? -1 : x > y;
}

int diary_load(const char *root, Diary *d)
{
    char *path = diary_path(root, 0), line[200];
    FILE *f;
    size_t cap = 0;

    d->days = NULL;
    d->n = 0;
    if (!path)
        return -1;
    f = fopen(path, "r");
    free(path);
    if (!f)
        return errno == ENOENT ? 0 : -1;
    while (fgets(line, sizeof line, f)) {
        long a, b;
        char *c1 = strchr(line, ','), *c2;
        long day;

        if (!c1)
            continue;
        *c1 = '\0';
        day = diary_parse_day(line);
        if (day < 0)
            continue;
        a = strtol(c1 + 1, &c2, 10);
        if (*c2 != ',')
            continue;
        b = strtol(c2 + 1, NULL, 10);
        if (d->n == cap) {
            DiaryDay *nd = realloc(d->days, (cap ? cap * 2 : 64) * sizeof *nd);

            if (!nd) {
                fclose(f);
                diary_free(d);
                return -1;
            }
            d->days = nd;
            cap = cap ? cap * 2 : 64;
        }
        d->days[d->n++] = (DiaryDay){ day, a, b };
    }
    fclose(f);
    if (d->n > 1)
        qsort(d->days, d->n, sizeof *d->days, cmp_day);
    /* un dia repetit: es queda l'últim */
    for (size_t i = 1; i < d->n;)
        if (d->days[i].day == d->days[i - 1].day) {
            d->days[i - 1] = d->days[i];
            memmove(&d->days[i], &d->days[i + 1], (d->n - i - 1) * sizeof *d->days);
            d->n--;
        } else
            i++;
    return 0;
}

void diary_free(Diary *d)
{
    free(d->days);
    d->days = NULL;
    d->n = 0;
}

int diary_record(const char *root, long words, time_t now)
{
    Diary d;
    char *dir, *path, *buf;
    long today = diary_day_of(now);
    size_t i, len = 0;
    int fd, ok = 0;
    FILE *f;

    if (diary_load(root, &d) != 0)
        return -1;
    if (d.n && d.days[d.n - 1].day == today) {
        if (d.days[d.n - 1].end == words) {         /* res a canviar */
            diary_free(&d);
            return 0;
        }
        d.days[d.n - 1].end = words;
    } else {
        DiaryDay *nd = realloc(d.days, (d.n + 1) * sizeof *nd);

        if (!nd) {
            diary_free(&d);
            return -1;
        }
        d.days = nd;
        d.days[d.n] = (DiaryDay){ today, d.n && d.days[d.n - 1].day < today ? d.days[d.n - 1].end : words, words };
        d.n++;
    }
    dir = diary_path(root, 1);
    path = diary_path(root, 0);
    buf = malloc(d.n * 48 + 1);
    if (!dir || !path || !buf) {
        free(dir); free(path); free(buf);
        diary_free(&d);
        return -1;
    }
    for (i = 0; i < d.n; i++) {
        char ds[16];

        diary_day_str(d.days[i].day, ds, sizeof ds);
        len += (size_t)snprintf(buf + len, 48, "%s,%ld,%ld\n", ds, d.days[i].start, d.days[i].end);
    }
    diary_free(&d);
    if (mkdir(dir, 0777) != 0 && errno != EEXIST) {
        free(dir); free(path); free(buf);
        return -1;
    }
    {
        size_t tl = strlen(path) + 32;
        char *tmp = malloc(tl);

        snprintf(tmp, tl, "%s.tmp.%ld", path, (long)getpid());
        fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        f = fd >= 0 ? fdopen(fd, "wb") : NULL;
        if (f) {
            ok = fwrite(buf, 1, len, f) == len && fflush(f) == 0 && fsync(fd) == 0;
            ok = fclose(f) == 0 && ok;
            ok = ok && rename(tmp, path) == 0;
        } else if (fd >= 0) {
            close(fd);
        }
        if (!ok)
            unlink(tmp);
        free(tmp);
    }
    free(dir); free(path); free(buf);
    return ok ? 0 : -1;
}

void diary_stats(const Diary *d, long total, long objectiu, const char *data_limit, time_t now,
                 DiaryStats *s)
{
    long today = diary_day_of(now), sum30 = 0, first = -1;

    memset(s, 0, sizeof *s);
    s->remaining = -1;
    s->days_to_goal = -1;
    s->finish_day = -1;
    s->limit_day = data_limit && *data_limit ? diary_parse_day(data_limit) : -1;
    for (size_t i = 0; i < d->n; i++) {
        long w = d->days[i].end - d->days[i].start, age = today - d->days[i].day;

        if (first < 0)
            first = d->days[i].day;
        if (w > 0)
            s->active_days++;
        if (w > s->best)
            s->best = w;
        if (age >= 0 && age < 7)
            s->week[6 - age] = w;
        if (age >= 0 && age < 30 && w > 0)
            sum30 += w;
    }
    for (int i = 0; i < 7; i++)
        s->week_total += s->week[i];
    s->today = s->week[6];
    /* ratxa: dies seguits amb feina; si avui encara res, es compta des d'ahir */
    for (long day = s->today > 0 ? today : today - 1;; day--) {
        size_t i;

        for (i = 0; i < d->n; i++)
            if (d->days[i].day == day && d->days[i].end - d->days[i].start > 0)
                break;
        if (i == d->n)
            break;
        s->streak++;
    }
    if (first >= 0) {
        long win = today - first + 1;

        if (win > 30)
            win = 30;
        if (win < 1)
            win = 1;
        s->avg = sum30 / win;
    }
    if (objectiu > 0) {
        s->remaining = objectiu - total > 0 ? objectiu - total : 0;
        if (s->remaining == 0)
            s->days_to_goal = 0;
        else if (s->avg > 0) {
            s->days_to_goal = (s->remaining + s->avg - 1) / s->avg;
            s->finish_day = today + s->days_to_goal;
        }
    }
}

static char *fmt(const char *f, ...) __attribute__((format(printf, 1, 2)));
static char *fmt(const char *f, ...)
{
    va_list ap;
    char buf[256];

    va_start(ap, f);
    vsnprintf(buf, sizeof buf, f, ap);
    va_end(ap);
    return strdup(buf);
}

static void num(long n, char *out, size_t sz)
{
    char t[32];
    size_t l, j = 0;
    int neg = n < 0;

    snprintf(t, sizeof t, "%ld", neg ? -n : n);
    l = strlen(t);
    if (neg && j + 1 < sz)
        out[j++] = '-';
    for (size_t i = 0; i < l && j + 2 < sz; i++) {
        if (i && (l - i) % 3 == 0)
            out[j++] = '.';
        out[j++] = t[i];
    }
    out[j] = '\0';
}

static const char *const WD[7] = { "dg", "dl", "dt", "dc", "dj", "dv", "ds" };

char **diary_report(const Diary *d, long total, long objectiu, const char *data_limit, time_t now,
                    size_t *nlines)
{
    DiaryStats s;
    char **l = calloc(24, sizeof *l), a[32], b[32];
    size_t n = 0;
    long today = diary_day_of(now), mx = 1;

    *nlines = 0;
    if (!l)
        return NULL;
    diary_stats(d, total, objectiu, data_limit, now, &s);
    num(s.today, a, sizeof a);
    num(total, b, sizeof b);
    l[n++] = fmt("Avui: %s paraules · manuscrit: %s paraules", a, b);
    num(s.week_total, a, sizeof a);
    num(s.avg, b, sizeof b);
    l[n++] = fmt("Setmana: %s · mitjana diària (30 dies): %s", a, b);
    num(s.best, a, sizeof a);
    l[n++] = fmt("Ratxa: %d %s · dies amb feina: %d · millor dia: %s", s.streak,
                 s.streak == 1 ? "dia" : "dies", s.active_days, a);
    l[n++] = fmt("%s", "");
    for (int i = 0; i < 7; i++)
        if (s.week[i] > mx)
            mx = s.week[i];
    for (int i = 0; i < 7; i++) {
        long day = today - (6 - i);
        struct tm tm;
        time_t t = now - (time_t)(6 - i) * 86400;
        char bar[100] = "", ds[16];
        int w = (int)(s.week[i] > 0 ? (s.week[i] * 30 + mx - 1) / mx : 0);

        localtime_r(&t, &tm);
        diary_day_str(day, ds, sizeof ds);
        for (int k = 0; k < w && k < 30; k++)
            strcat(bar, "█");
        num(s.week[i], a, sizeof a);
        l[n++] = fmt("  %s %s %7s %s", WD[tm.tm_wday], ds + 5, a, bar);
    }
    l[n++] = fmt("%s", "");
    if (objectiu <= 0) {
        l[n++] = fmt("%s", "Previsió: sense objectiu al projecte.md.");
    } else if (s.remaining == 0) {
        l[n++] = fmt("%s", "Objectiu assolit.");
    } else {
        num(s.remaining, a, sizeof a);
        if (s.finish_day < 0) {
            l[n++] = fmt("Falten %s paraules; amb la mitjana actual no es pot preveure.", a);
        } else {
            char fd[16];

            diary_day_str(s.finish_day, fd, sizeof fd);
            l[n++] = fmt("Falten %s paraules: a la mitjana actual, el %s (en %ld dies).", a, fd,
                         s.days_to_goal);
        }
        if (s.limit_day >= 0) {
            long left = s.limit_day - today;

            if (left < 0) {
                l[n++] = fmt("%s", "Data límit: ja passada.");
            } else {
                long need = (s.remaining + (left ? left : 1) - 1) / (left ? left : 1);

                num(need, a, sizeof a);
                l[n++] = fmt("Fins a la data límit (%ld dies): calen %s paraules al dia.", left, a);
                if (s.finish_day >= 0)
                    l[n++] = s.finish_day <= s.limit_day
                                 ? fmt("%s", "Al ritme actual s'hi arriba a temps.")
                                 : fmt("Al ritme actual s'arriba %ld dies tard.", s.finish_day - s.limit_day);
            }
        }
    }
    *nlines = n;
    return l;
}
