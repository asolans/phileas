#include "retalls.h"

#include "util.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_RETALLS (16u << 20)   /* 16 MB: més seria un error */

static void path_of(char *out, size_t size)
{
    char dir[1024];

    config_dir(dir, sizeof dir);
    snprintf(out, size, "%s/retalls", dir);
}

int cm_retalls_write(const char *s, size_t n, struct stat *st)
{
    char path[1100], tmp[1200];
    FILE *f;
    int err = 0;

    path_of(path, sizeof path);
    snprintf(tmp, sizeof tmp, "%s.%ld", path, (long)getpid());
    {   /* el directori pot no existir encara (un usuari nou al PC) */
        char dir[1024];

        config_dir(dir, sizeof dir);
        mkdir(dir, 0755);
    }
    if (!(f = fopen(tmp, "w")))
        return -1;
    if (fwrite(s, 1, n, f) != n || fflush(f) != 0)
        err = errno ? errno : EIO;
    if (fclose(f) != 0 && !err)
        err = errno;
    if (!err && rename(tmp, path) != 0)
        err = errno;
    if (err) {
        unlink(tmp);
        errno = err;
        return -1;
    }
    if (st && stat(path, st) != 0)
        memset(st, 0, sizeof *st);
    return 0;
}

char *cm_retalls_read(size_t *n, struct stat *st)
{
    char path[1100], *buf;
    FILE *f;
    struct stat s;

    *n = 0;
    path_of(path, sizeof path);
    if (stat(path, &s) != 0 || !S_ISREG(s.st_mode) || s.st_size <= 0 ||
        (size_t)s.st_size > MAX_RETALLS || !(f = fopen(path, "rb")))
        return NULL;
    buf = xmalloc((size_t)s.st_size + 1);
    *n = fread(buf, 1, (size_t)s.st_size, f);
    fclose(f);
    buf[*n] = '\0';
    if (st)
        *st = s;
    if (!*n) {
        free(buf);
        return NULL;
    }
    return buf;
}

int cm_retalls_changed(const struct stat *st)
{
    char path[1100];
    struct stat s;

    path_of(path, sizeof path);
    if (stat(path, &s) != 0)
        return 0;
    return s.st_ino != st->st_ino || s.st_size != st->st_size ||
           s.st_mtim.tv_sec != st->st_mtim.tv_sec || s.st_mtim.tv_nsec != st->st_mtim.tv_nsec;
}
