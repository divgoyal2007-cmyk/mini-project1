
#include "resume.h"
#include "jobs.h"
#include "terminal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/wait.h>

static bool is_nonneg_int(const char *s) {
    if (s == NULL || *s == '\0') return false;
    for (const char *p = s; *p; p++) {
        if (*p < '0' || *p > '9') return false;
    }
    return true;
}

static volatile sig_atomic_t g_alarm_fired=0;
//onlj job interrupt the bolocking waitpid() when the timeout elapses 
static void alarm_handler(int sig){
    (void)sig;
    g_alarm_fired=1;
}

void execute_resume(Command *cmd) {
   
    if (cmd->argc < 3) { printf("resume: invalid syntax\n"); return; }
 
    const char *job_arg = cmd->argv[1];
    if (job_arg[0] != '%' || !is_nonneg_int(job_arg + 1)) {
        printf("resume: invalid syntax\n");   // missing/malformed job number
        return;
    }
    int job_number = atoi(job_arg + 1);
 
    const char *mode = cmd->argv[2];
    bool is_fg = (strcmp(mode, "fg") == 0);
    bool is_bg = (strcmp(mode, "bg") == 0);
    if (!is_fg && !is_bg) { printf("resume: invalid syntax\n"); return; }   // bad fg/bg
 
    bool has_timeout = false;
    int timeout_seconds = 0;
 
    if (cmd->argc == 5) {
        if (!is_fg || strcmp(cmd->argv[3], "--timeout") != 0 || !is_nonneg_int(cmd->argv[4])) {
            printf("resume: invalid syntax\n");   // timeout without a valid number
            return;
        }
        has_timeout = true;
        timeout_seconds = atoi(cmd->argv[4]);
    } else if (cmd->argc != 3) {
        printf("resume: invalid syntax\n");   // extra/missing tokens
        return;
    }
    Job *job = jobs_find_by_number(job_number);
    if (job == NULL) { printf("resume: no such job\n"); return; }   
 
    pid_t pgid = job->pgid;
    kill(-pgid, SIGCONT);   //  wake the whole group up
 
    if (is_bg) {
        jobs_mark_running(pgid);                                      
        printf("[%d] + Running    %s\n", job->job_number, job->cmdline); 
        fflush(stdout);
        return;   // never wait, never touch the terminal
    }
    printf("%s\n", job->cmdline);   // print command line before waiting
    fflush(stdout);
    jobs_mark_running(pgid);
 
    sigset_t old_mask;
    jobs_block_sigchld(&old_mask);   // same rule as any other fg wait
 
    terminal_give_to(pgid);   
 
    if (has_timeout) {
        g_alarm_fired = 0;
        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = alarm_handler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;   // no SA_RESTART -- must interrupt the wait below
        sigaction(SIGALRM, &sa, NULL);
        alarm((unsigned)timeout_seconds);   
    }
 
    bool stopped = false;
    bool timed_out = false;
 
    for (int i = 0; i < job->npids && !timed_out; i++) {
        if (job->pid_done[i]) continue;
        int status;
        pid_t w = waitpid(job->pids[i], &status, WUNTRACED);
        if (w < 0) {
            if (errno == EINTR && g_alarm_fired) { timed_out = true; break; }
            continue;
        }
        if (WIFSTOPPED(status)) { stopped = true; break; }
        job->pid_done[i] = true;
    }
 
    if (has_timeout) {
        alarm(0);                   // finished/stopped early -> cancel timer
        signal(SIGALRM, SIG_DFL);
    }
 
    terminal_reclaim();   
 
    if (timed_out) {
        kill(-pgid, SIGTERM);           
        printf("resume: job timed out\n");
        fflush(stdout);
        jobs_remove(pgid);               // terminated -- drop it entirely
    } else if (stopped) {
        jobs_mark_stopped(pgid);
        printf("[%d] + Stopped    %s\n", job->job_number, job->cmdline);
        fflush(stdout);
    } else {
        jobs_remove(pgid);               // finished normally
    }
 
    jobs_unblock_sigchld(&old_mask);
}
