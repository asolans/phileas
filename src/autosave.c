#include "autosave.h"
#include "util.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

double now_seconds(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

void autosave_tick(Editor *e, double now)
{
    time_t t;
    struct tm tm;

    if (!e->buf.modified) {
        e->dirty_since = 0;
        return;
    }
    if (e->dirty_since == 0)
        e->dirty_since = now;
    if (!e->autosave)
        return;
    if (!e->filename) {
        if (!e->autosave_warned) {
            ed_msg(e, "Autodesat: cal desar el document amb un nom (Alt+S)");
            e->autosave_warned = 1;
        }
        return;
    }
    if (now - e->last_input < e->autosave && now - e->dirty_since < e->autosave_max)
        return;

    if (buf_save(&e->buf, e->filename) < 0) {
        ed_msg(e, "Autodesat: no s'ha pogut desar %s: %s", e->filename, strerror(errno));
        e->dirty_since = now;   /* es tornarà a provar d'aquí a autosave_max */
        e->last_input = now;
        return;
    }
    t = time(NULL);
    localtime_r(&t, &tm);
    ed_msg(e, "Desat automàticament a les %02d:%02d", tm.tm_hour, tm.tm_min);
    e->dirty_since = 0;
}

int autosave_wait_ms(const Editor *e, double now)
{
    double due;

    if (!e->autosave || !e->filename || !e->buf.modified || e->dirty_since == 0)
        return -1;
    due = e->last_input + e->autosave;
    if (e->dirty_since + e->autosave_max < due)
        due = e->dirty_since + e->autosave_max;
    if (due <= now)
        return 0;
    return (int)((due - now) * 1000) + 10;
}

int autosave_emergency(Editor *e, char *path, size_t size)
{
    char dir[1024];
    const char *home = getenv("HOME");
    time_t t;
    struct tm tm;

    path[0] = '\0';
    if (!e->buf.modified)
        return 0;
    if (e->filename && buf_save(&e->buf, e->filename) == 0) {
        snprintf(path, size, "%s", e->filename);
        return 0;
    }
    t = time(NULL);
    localtime_r(&t, &tm);
    config_dir(dir, sizeof dir);
    mkdir(dir, 0700);   /* si ja existeix, res */
    for (int i = 0; i < 2; i++) {
        snprintf(path, size, "%s/rescat-%04d%02d%02d-%02d%02d%02d.%s", i ? home : dir,
                 tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min,
                 tm.tm_sec, e->buf.plain ? "txt" : "md");
        if ((i == 0 || (home && *home)) && buf_save(&e->buf, path) == 0)
            return 0;
    }
    path[0] = '\0';
    return -1;
}
