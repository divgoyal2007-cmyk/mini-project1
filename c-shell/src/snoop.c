#include "snoop.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef __linux__
#include "jobs.h"
#include <string.h>
#include <unistd.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <time.h>
#include <stdbool.h>

#define MAX_SYSCALLS 1024

typedef struct {
    int id;
    int count;
    double total_time;
    int first_occurrence;
} SyscallStat;

static const char *get_syscall_name(int id) {
    switch (id) {
        case 0: return "read";
        case 1: return "write";
        case 2: return "open";
        case 3: return "close";
        case 35: return "nanosleep";
        case 231: return "exit_group";
        default: return NULL;
    }
}

static int compare_stats(const void *a, const void *b) {
    const SyscallStat *sa = (const SyscallStat *)a;
    const SyscallStat *sb = (const SyscallStat *)b;
    if (sa->count != sb->count) return sb->count - sa->count;   // descending by count
    return sa->first_occurrence - sb->first_occurrence;          // tie: first occurrence wins
}

static double get_time_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + (ts.tv_nsec / 1e9);
}

void execute_snoop(Command *cmd) {
    if (cmd->argc < 2) {
        printf("snoop: invalid syntax\n");
        return;
    }

    // Block SIGCHLD for the whole tracing session. Our shell already
    // installs a GLOBAL SIGCHLD handler (jobs.c) that also calls
    // waitpid(-1, ...); ptrace stops generate SIGCHLD just like exits do,
    // so without this, that handler could "steal" a state-change meant
    // for our own explicit waitpid(pid, ...) calls below and hang us.
    sigset_t old_mask;
    jobs_block_sigchld(&old_mask);

    pid_t pid;
    int initial_status;

    if (strcmp(cmd->argv[1], "-p") == 0) {
        if (cmd->argc != 3) {
            printf("snoop: invalid syntax\n");
            jobs_unblock_sigchld(&old_mask);
            return;
        }
        pid = (pid_t)atoi(cmd->argv[2]);
        if (ptrace(PTRACE_ATTACH, pid, NULL, NULL) < 0) {
            printf("snoop: no such process\n");
            jobs_unblock_sigchld(&old_mask);
            return;
        }
        waitpid(pid, &initial_status, 0);
    } else {
        pid = fork();
        if (pid == 0) {
            ptrace(PTRACE_TRACEME, 0, NULL, NULL);
            execvp(cmd->argv[1], &cmd->argv[1]);
            printf("snoop: command not found\n");
            exit(127);
        }
        waitpid(pid, &initial_status, 0);
    }

    // If exec failed, the child ran straight to exit() -- no ptrace stop
    // ever happened, so there's nothing to trace and no summary to print.
    if (WIFEXITED(initial_status) || WIFSIGNALED(initial_status)) {
        jobs_unblock_sigchld(&old_mask);
        return;
    }

    SyscallStat stats[MAX_SYSCALLS] = {0};
    bool in_syscall = false;
    double entry_time = 0;
    int order = 0;

    while (1) {
        struct user_regs_struct regs;
        ptrace(PTRACE_SYSCALL, pid, NULL, NULL);
        int status;
        pid_t w = waitpid(pid, &status, 0);
        if (w < 0) break;   // child gone -- don't read uninitialized status

        if (WIFEXITED(status) || WIFSIGNALED(status)) break;

        ptrace(PTRACE_GETREGS, pid, NULL, &regs);
        long orig_rax = regs.orig_rax;

        if (orig_rax >= 0 && orig_rax < MAX_SYSCALLS) {
            if (!in_syscall) {
                entry_time = get_time_sec();
                in_syscall = true;
            } else {
                double exit_time = get_time_sec();
                stats[orig_rax].id = (int)orig_rax;
                stats[orig_rax].count++;
                stats[orig_rax].total_time += (exit_time - entry_time);
                if (stats[orig_rax].first_occurrence == 0) {
                    stats[orig_rax].first_occurrence = ++order;
                }
                in_syscall = false;
            }
        }
    }

    jobs_unblock_sigchld(&old_mask);   // safe to let the shell's handler run again now

    SyscallStat valid_stats[MAX_SYSCALLS];
    int valid_count = 0;
    for (int i = 0; i < MAX_SYSCALLS; i++) {
        if (stats[i].count > 0) valid_stats[valid_count++] = stats[i];
    }
    qsort(valid_stats, valid_count, sizeof(SyscallStat), compare_stats);

    printf("%-20s %-10s %s\n", "syscall", "calls", "time");
    for (int i = 0; i < valid_count; i++) {
        const char *name = get_syscall_name(valid_stats[i].id);
        char unknown[32];
        if (!name) {
            snprintf(unknown, sizeof(unknown), "syscall_%d", valid_stats[i].id);
            name = unknown;
        }
        printf("%-20s %-10d %.3fs\n", name, valid_stats[i].count, valid_stats[i].total_time);
    }
}

#else

void execute_snoop(Command *cmd) {
    (void)cmd;
    printf("snoop: this command requires Linux ptrace and cannot be run on macOS.\n");
}
#endif