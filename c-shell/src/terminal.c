#include "terminal.h"
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

static int shell_terminal;   // fd of the controlling terminal
static pid_t shell_pgid;     // shell's own process group id

// E2 #1: shell must survive Ctrl-C / Ctrl-Z, not die or stop.
// No SA_RESTART on purpose -- we WANT fgets() interrupted so the
// main loop notices and redraws the prompt instead of silently resuming.
static void shell_signal_handler(int sig) {
    (void)sig;
    printf("\n");
    fflush(stdout);
}

void terminal_init(void) {
    // Prefer the real tty device over STDIN_FILENO -- more robust in case
    // stdin isn't literally the controlling terminal for some reason.
    int tty_fd = open("/dev/tty", O_RDWR);
    shell_terminal = (tty_fd >= 0) ? tty_fd : STDIN_FILENO;

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = shell_signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);    // E2 #1
    sigaction(SIGTSTP, &sa, NULL);   // E2 #1

    signal(SIGTTOU, SIG_IGN);   // E2 #1: our own tcsetpgrp() must not stop us
    signal(SIGTTIN, SIG_IGN);   // shell itself never does background reads

    shell_pgid = getpid();
    if (setpgid(shell_pgid, shell_pgid) < 0 && errno != EPERM) {
        fprintf(stderr, "cshell: setpgid(init) failed: %s\n", strerror(errno));
    }
    if (tcsetpgrp(shell_terminal, shell_pgid) < 0) {
        fprintf(stderr, "cshell: tcsetpgrp(init) failed: %s\n", strerror(errno));
    }
}

void terminal_give_to(pid_t pgid) {
    if (tcsetpgrp(shell_terminal, pgid) < 0) {
        fprintf(stderr, "cshell: tcsetpgrp(give %d) failed: %s\n", (int)pgid, strerror(errno));
    }
}

void terminal_reclaim(void) {
    if (tcsetpgrp(shell_terminal, shell_pgid) < 0) {
        fprintf(stderr, "cshell: tcsetpgrp(reclaim) failed: %s\n", strerror(errno));
    }
}

pid_t terminal_shell_pgid(void) {
    return shell_pgid;
}