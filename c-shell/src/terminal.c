
#include "terminal.h"
#include <unistd.h>
#include <termios.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>

static int shell_terminal;

static pid_t shell_pgid;

static void shell_signal_handler(int sig){
    (void)sig;
    printf("\n");
    fflush(stdout);
}

void terminal_init(void){
    shell_terminal=STDIN_FILENO;
    struct sigaction sa;
    memset(&sa,0,sizeof(sa));
    sa.sa_handler = shell_signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags=0;
    sigaction(SIGINT,&sa,NULL);
    sigaction(SIGTSTP,&sa,NULL);

    signal(SIGTTOU,SIG_IGN);

    signal(SIGTTIN, SIG_IGN);

    shell_pgid = getpid();
    setpgid(shell_pgid, shell_pgid);

    tcsetpgrp(shell_terminal,shell_pgid);
}

void terminal_give_to(pid_t pgid){
    tcsetpgrp(shell_terminal,pgid);
}
void terminal_reclaim(void){
    tcsetpgrp(shell_terminal,shell_pgid);
}
pid_t terminal_shell_pgid(void){
    return shell_pgid;
}