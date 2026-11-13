#include "launch.h"
#include "fterm.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_ARGS 12

int launch_editor(const char *path, size_t line, char *err, size_t errsz)
{
    const char *env = getenv("FABULA_EDITOR");
    char *cmd = strdup(env && *env ? env : "phileas"), plus[32];
    char *argv[MAX_ARGS + 3];
    int n = 0, status = 0;
    pid_t pid;

    if (!cmd) {
        snprintf(err, errsz, "sense memòria");
        return -1;
    }
    for (char *t = strtok(cmd, " \t"); t && n < MAX_ARGS; t = strtok(NULL, " \t"))
        argv[n++] = t;
    if (n == 0) {
        free(cmd);
        snprintf(err, errsz, "FABULA_EDITOR és buit");
        return -1;
    }
    if (line) {
        snprintf(plus, sizeof plus, "+%zu", line);
        argv[n++] = plus;
    }
    argv[n++] = (char *)path;
    argv[n] = NULL;

    ft_suspend();
    pid = fork();
    if (pid == 0) {
        execvp(argv[0], argv);
        _exit(127);
    }
    if (pid > 0)
        while (waitpid(pid, &status, 0) < 0 && errno == EINTR)
            ;
    ft_resume();
    {
        int rc = pid < 0 ? -1 : WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);

        if (pid < 0)
            snprintf(err, errsz, "no s'ha pogut crear el procés: %s", strerror(errno));
        else if (rc == 127)
            snprintf(err, errsz, "no s'ha pogut executar «%s» (es pot canviar amb FABULA_EDITOR)", argv[0]);
        else if (rc != 0)
            snprintf(err, errsz, "«%s» ha acabat amb el codi %d", argv[0], rc);
        else
            rc = 0;
        free(cmd);
        return rc == 0 ? 0 : -1;
    }
}

int launch_program(char *const argv[], char *err, size_t errsz)
{
    int status = 0, rc;
    pid_t pid;

    ft_suspend();
    pid = fork();
    if (pid == 0) {
        execvp(argv[0], argv);
        _exit(127);
    }
    if (pid > 0)
        while (waitpid(pid, &status, 0) < 0 && errno == EINTR)
            ;
    ft_resume();
    if (pid < 0) {
        snprintf(err, errsz, "no s'ha pogut crear el procés: %s", strerror(errno));
        return -1;
    }
    rc = WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
    if (rc == 127) {
        snprintf(err, errsz, "no s'ha pogut executar «%s»", argv[0]);
        return -1;
    }
    if (rc != 0) {
        snprintf(err, errsz, "«%s» ha acabat amb el codi %d", argv[0], rc);
        return -1;
    }
    return 0;
}
