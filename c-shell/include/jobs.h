#ifndef JOBS_H
#define JOBS_H

#include <sys/types.h>
#include <signal.h>
#include <stdbool.h>
#include "command.h"

#define MAX_JOBS     256
#define MAX_JOB_PIDS 64
#define MAX_CMDLINE  256
#define MAX_NAME     64

typedef enum { JOB_RUNNING, JOB_STOPPED } JobState;

typedef struct {
    int      job_number;                          // never reused, always increasing
    pid_t    pgid;                                 // == pids[0], the whole group's id
    pid_t    pids[MAX_JOB_PIDS];                   // every process in this pipeline
    char     stage_names[MAX_JOB_PIDS][MAX_NAME];  // each stage's program name
    bool     pid_done[MAX_JOB_PIDS];               // has this specific pid exited?
    int      npids;
    char     cmdline[MAX_CMDLINE];                 // full display string, e.g. "cat | sort"
    JobState state;
    bool     background;                           // launched with '&' from the start?
    bool     active;                                // still tracked at all?
} Job;

void jobs_init(void);                                    // installs SIGCHLD handler

int  jobs_add(const pid_t *pids, Pipeline *p, bool background); // register a new job

void jobs_mark_stopped(pid_t pgid);   // e.g. after Ctrl-Z
void jobs_mark_running(pid_t pgid);   // e.g. after SIGCONT (resume)
void jobs_remove(pid_t pgid);         // stop tracking entirely

Job *jobs_find_by_number(int job_number);   // for resume/ping "%N"
Job *jobs_find_by_pgid(pid_t pgid);
Job *jobs_find_by_pid(pid_t pid);           // any pid inside any tracked job

int  jobs_total_slots(void);   // raw table size, for iterating (activities)
Job *jobs_get(int index);      // may return an inactive job -- check ->active

bool jobs_any_stopped(void);   // for Ctrl-D's "there are stopped jobs" check
void jobs_hangup_all(void);    // SIGHUP every tracked job's group, don't wait

void jobs_block_sigchld(sigset_t *old_mask);
void jobs_unblock_sigchld(const sigset_t *old_mask);

#endif