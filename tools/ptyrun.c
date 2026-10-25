/* Eina de proves (no forma part de Phileas). A la Pi, el servidor SSH
 * (dropbear) no obre terminals: ptyrun hi crea un pseudoterminal. Ús:
 *   ssh root@pi "cd /tmp/phileas && TERM=linux ./ptyrun ./phileas fitxer.md"
 * Compilació creuada: arm-linux-gcc -O2 -o ptyrun ptyrun.c -lutil */
/* ptyrun: executa una ordre en un pseudoterminal de 80x24 i hi passa stdin/stdout. */
#define _DEFAULT_SOURCE
#include <pty.h>
#include <stdio.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    struct winsize ws = { 24, 80, 0, 0 };
    int fd, in_open = 1;
    pid_t pid;
    char buf[65536];

    if (argc < 2)
        return 2;
    pid = forkpty(&fd, NULL, NULL, &ws);
    if (pid < 0) { perror("forkpty"); return 1; }
    if (pid == 0) { execvp(argv[1], argv + 1); perror("exec"); _exit(127); }
    for (;;) {
        fd_set r;
        ssize_t n;

        FD_ZERO(&r);
        FD_SET(fd, &r);
        if (in_open) FD_SET(0, &r);
        if (select(fd + 1, &r, NULL, NULL, NULL) < 0) break;
        if (FD_ISSET(fd, &r)) {
            if ((n = read(fd, buf, sizeof buf)) <= 0) break;
            if (write(1, buf, (size_t)n) < 0) break;
        }
        if (in_open && FD_ISSET(0, &r)) {
            if ((n = read(0, buf, sizeof buf)) <= 0) in_open = 0;
            else if (write(fd, buf, (size_t)n) < 0) break;
        }
    }
    waitpid(pid, NULL, 0);
    return 0;
}
