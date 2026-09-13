#include "jobs.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>
#include <unistd.h>

static Job jobs[MAX_JOBS];
static int njobs = 0;
static int next_job_number = 1;   // only ever increments -> never reused

// find which job (and which slot within it) owns this pid
static Job *find_job_for_pid(pid_t pid, int *slot) {
    for (int i = 0; i < njobs; i++) {
        if (!jobs[i].active) continue;
        for (int k = 0; k < jobs[i].npids; k++) {
            if (jobs[i].pids[k] == pid) {
                *slot = k;
                return &jobs[i];
            }
        }
    }
    return NULL;
}

static void sigchld_handler(int sig) {
    (void)sig;
    int saved_errno = errno;   // don't clobber errno for interrupted code
    int status;
    pid_t pid;

    // drain every pending state change, non-blocking
    // WUNTRACED  -> also notice a child getting Stopped (e.g. SIGTTIN, Ctrl-Z)
    // WCONTINUED -> also notice a child being resumed
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        int slot;
        Job *j = find_job_for_pid(pid, &slot);
        if (j == NULL) continue;

        if (WIFSTOPPED(status)) {
            j->state = JOB_STOPPED;
            // only print here for jobs that were ALREADY background --
            // a foreground job's Ctrl-Z stop is printed by sys_command.c
            // itself, to avoid printing it twice
            if (slot == 0 && j->background) {
                printf("[%d] + Stopped    %s\n", j->job_number, j->cmdline);
                fflush(stdout);
            }
        } else if (WIFCONTINUED(status)) {
            j->state = JOB_RUNNING;
        } else {
            // real exit (normal or killed by signal)
            j->pid_done[slot] = true;
            if (slot == 0) {
                if (WIFEXITED(status)) {
                    printf("%s with pid %d exited normally\n", j->stage_names[0], (int)pid);
                } else if (WIFSIGNALED(status)) {
                    printf("%s with pid %d exited abnormally\n", j->stage_names[0], (int)pid);
                }
                fflush(stdout);
                j->active = false;   // whole job done, drop it from tracking
            }
        }
    }
    errno = saved_errno;
}

void jobs_init(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;   // let fgets() resume after the handler runs
    sigaction(SIGCHLD, &sa, NULL);
}

int jobs_add(const pid_t *pids, Pipeline *p, bool background) {
    if (njobs >= MAX_JOBS || p == NULL || p->nstages <= 0) return -1;

    Job *j = &jobs[njobs++];
    j->job_number = next_job_number++;
    j->pgid = pids[0];
    j->npids = (p->nstages > MAX_JOB_PIDS) ? MAX_JOB_PIDS : p->nstages;

    for (int i = 0; i < j->npids; i++) {
        j->pids[i] = pids[i];
        j->pid_done[i] = false;
        snprintf(j->stage_names[i], MAX_NAME, "%s", p->stages[i]->name);
    }

    // build a display string like "sleep 100" or "cat | sort"
    size_t used = 0;
    j->cmdline[0] = '\0';
    for (int s = 0; s < p->nstages; s++) {
        if (s > 0) {
            int w = snprintf(j->cmdline + used, sizeof(j->cmdline) - used, " | ");
            if (w > 0) used += (size_t)w;
        }
        Command *cmd = p->stages[s];
        for (int a = 0; a < cmd->argc; a++) {
            int w = snprintf(j->cmdline + used, sizeof(j->cmdline) - used,
                              "%s%s", (a > 0 ? " " : ""), cmd->argv[a]);
            if (w > 0) used += (size_t)w;
        }
    }

    j->state = JOB_RUNNING;
    j->background = background;
    j->active = true;
    return j->job_number;
}

void jobs_mark_stopped(pid_t pgid) {
    Job *j = jobs_find_by_pgid(pgid);
    if (j != NULL) j->state = JOB_STOPPED;
}

void jobs_mark_running(pid_t pgid) {
    Job *j = jobs_find_by_pgid(pgid);
    if (j != NULL) j->state = JOB_RUNNING;
}

void jobs_remove(pid_t pgid) {
    Job *j = jobs_find_by_pgid(pgid);
    if (j != NULL) j->active = false;
}

Job *jobs_find_by_number(int job_number) {
    for (int i = 0; i < njobs; i++) {
        if (jobs[i].active && jobs[i].job_number == job_number) return &jobs[i];
    }
    return NULL;
}

Job *jobs_find_by_pgid(pid_t pgid) {
    for (int i = 0; i < njobs; i++) {
        if (jobs[i].active && jobs[i].pgid == pgid) return &jobs[i];
    }
    return NULL;
}

Job *jobs_find_by_pid(pid_t pid) {
    for (int i = 0; i < njobs; i++) {
        if (!jobs[i].active) continue;
        for (int k = 0; k < jobs[i].npids; k++) {
            if (jobs[i].pids[k] == pid) return &jobs[i];
        }
    }
    return NULL;
}

int jobs_total_slots(void) { return njobs; }

Job *jobs_get(int index) {
    if (index < 0 || index >= njobs) return NULL;
    return &jobs[index];
}

bool jobs_any_stopped(void) {
    for (int i = 0; i < njobs; i++) {
        if (jobs[i].active && jobs[i].state == JOB_STOPPED) return true;
    }
    return false;
}

void jobs_hangup_all(void) {
    for (int i = 0; i < njobs; i++) {
        if (jobs[i].active) {
            kill(-jobs[i].pgid, SIGHUP);   // negative pid == whole process group
        }
    }
}

void jobs_block_sigchld(sigset_t *old_mask) {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGCHLD);
    sigprocmask(SIG_BLOCK, &set, old_mask);
}

void jobs_unblock_sigchld(const sigset_t *old_mask) {
    sigprocmask(SIG_SETMASK, old_mask, NULL);
}