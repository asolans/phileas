#include "clipboard.h"
#include "util.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/* Temps màxim d'espera de l'eina, perquè un porta-retalls encallat no
 * bloquegi l'editor */
#define TIMEOUT_MS 1500

typedef struct {
    const char *name;
    const char *env;            /* variable que indica l'escriptori */
    const char *const *set;
    const char *const *get;
} Tool;

static const Tool tools[] = {
    { "wl-clipboard", "WAYLAND_DISPLAY",
      (const char *const[]){ "wl-copy", NULL },
      (const char *const[]){ "wl-paste", "--no-newline", "--type", "text", NULL } },
    { "xclip", "DISPLAY",
      (const char *const[]){ "xclip", "-selection", "clipboard", NULL },
      (const char *const[]){ "xclip", "-o", "-selection", "clipboard", "-target", "UTF8_STRING", NULL } },
    { "xsel", "DISPLAY",
      (const char *const[]){ "xsel", "--input", "--clipboard", NULL },
      (const char *const[]){ "xsel", "--output", "--clipboard", NULL } },
};
#define NTOOLS (sizeof tools / sizeof tools[0])

static int in_path(const char *prog)
{
    const char *path = getenv("PATH");
    char buf[1024];

    if (!path)
        path = "/usr/bin:/bin";
    while (*path) {
        size_t len = strcspn(path, ":");

        if (len && len + strlen(prog) + 2 <= sizeof buf) {
            snprintf(buf, sizeof buf, "%.*s/%s", (int)len, path, prog);
            if (access(buf, X_OK) == 0)
                return 1;
        }
        path += len;
        if (*path == ':')
            path++;
    }
    return 0;
}

static const Tool *detect(void)
{
    static int done;
    static const Tool *tool;
    size_t i;

    if (done)
        return tool;
    done = 1;
    for (i = 0; i < NTOOLS; i++) {
        const char *v = getenv(tools[i].env);

        if (v && *v && in_path(tools[i].set[0])) {
            tool = &tools[i];
            break;
        }
    }
    return tool;
}

const char *sysclip_tool(void)
{
    const Tool *t = detect();

    return t ? t->name : NULL;
}

static long now_ms(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

/* Executa argv amb l'entrada i la sortida a les canonades donades (-1 =
 * /dev/null). La sortida d'errors sempre va a /dev/null, per no embrutar
 * la pantalla. setsid: el procés que queda servint el porta-retalls no ha
 * de morir quan es tanqui el terminal. */
static pid_t spawn(const char *const *argv, int in, int out)
{
    pid_t pid = fork();

    if (pid == 0) {
        int null = open("/dev/null", O_RDWR);

        setsid();
        dup2(in >= 0 ? in : null, 0);
        dup2(out >= 0 ? out : null, 1);
        dup2(null, 2);
        signal(SIGPIPE, SIG_DFL);
        execvp(argv[0], (char *const *)argv);
        _exit(127);
    }
    return pid;
}

/* Canonada que no hereten els processos fills: si l'eina tingués oberta la
 * punta d'escriptura, no veuria mai el final del text */
static int cloexec_pipe(int fd[2])
{
    if (pipe(fd) < 0)
        return -1;
    fcntl(fd[0], F_SETFD, FD_CLOEXEC);
    fcntl(fd[1], F_SETFD, FD_CLOEXEC);
    return 0;
}

/* Espera el procés fins al límit; si no acaba, el mata. */
static int reap(pid_t pid, long deadline)
{
    int st;

    for (;;) {
        pid_t r = waitpid(pid, &st, WNOHANG);

        if (r == pid)
            return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
        if (r < 0 && errno != EINTR)
            return -1;
        if (now_ms() >= deadline) {
            kill(pid, SIGKILL);
            waitpid(pid, &st, 0);
            return -1;
        }
        usleep(5000);
    }
}

int sysclip_set(const char *s, size_t n)
{
    const Tool *t = detect();
    void (*old)(int);
    long deadline;
    int fd[2];
    pid_t pid;
    size_t off = 0;

    if (!t || cloexec_pipe(fd) < 0)
        return -1;
    old = signal(SIGPIPE, SIG_IGN);
    pid = spawn(t->set, fd[0], -1);
    close(fd[0]);
    if (pid < 0) {
        close(fd[1]);
        signal(SIGPIPE, old);
        return -1;
    }
    deadline = now_ms() + TIMEOUT_MS;
    while (off < n) {
        ssize_t w = write(fd[1], s + off, n - off);

        if (w < 0 && errno == EINTR)
            continue;
        if (w <= 0)
            break;
        off += (size_t)w;
    }
    close(fd[1]);
    signal(SIGPIPE, old);
    /* wl-copy i xclip llegeixen el text, es posen en segon pla per servir-lo
     * i el primer procés acaba */
    return reap(pid, deadline) == 0 && off == n ? 0 : -1;
}

char *sysclip_get(size_t *n)
{
    const Tool *t = detect();
    char *buf = NULL;
    size_t len = 0, cap = 0;
    long deadline;
    int fd[2], ok = 1;
    pid_t pid;

    *n = 0;
    if (!t || cloexec_pipe(fd) < 0)
        return NULL;
    pid = spawn(t->get, -1, fd[1]);
    close(fd[1]);
    if (pid < 0) {
        close(fd[0]);
        return NULL;
    }
    deadline = now_ms() + TIMEOUT_MS;
    for (;;) {
        struct pollfd p = { fd[0], POLLIN, 0 };
        long left = deadline - now_ms();
        ssize_t r;

        if (left <= 0 || poll(&p, 1, (int)left) == 0) {
            ok = 0;
            break;
        }
        if (len + 4096 > cap) {
            cap = cap ? cap * 2 : 8192;
            buf = xrealloc(buf, cap);
        }
        r = read(fd[0], buf + len, cap - len);
        if (r < 0 && (errno == EINTR || errno == EAGAIN))
            continue;
        if (r <= 0)
            break;
        len += (size_t)r;
    }
    close(fd[0]);
    if (reap(pid, ok ? deadline : 0) != 0 || !ok || len == 0) {
        free(buf);
        return NULL;
    }
    *n = len;
    return buf;
}
