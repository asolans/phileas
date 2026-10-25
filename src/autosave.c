#include "autosave.h"

#include <errno.h>
#include <string.h>
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
