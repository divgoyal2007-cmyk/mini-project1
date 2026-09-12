#include "jobs.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct
{
   int job_number;
   pid_t pids[max_jobs_pids];
   bool pid_done[max_jobs_pids];
   int npids;
   char cmdline[256];
   bool active;
}Job;

static Job jobs[max_jobs];
static int njobs=0;
static int next_job_number=1;

static Job *find_job_for_pid(pid_t pid,int *slot){
    for(int i=0;i<njobs;i++){
        for(int k=0; k< jobs[i].npids ;k++){
            if(jobs[i].pids[k]==pid && !jobs[i].pid_done[k]){
                *slot=k;
                return &jobs[i];
            }
        }
    }
    return NULL;
}

static void sigchild_handler(int sig){
    (void)sig;
    int saved_errno=errno;
    int status;
    pid_t pid;
    //sognal while it was blocked
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        int slot;
        Job* j=find_job_for_pid(pid,&slot);
        if(j==NULL){
            continue; //not one the tracked background pids
        }
        j->pid_done[slot]=true;
        if(slot==0){ //this is the jobs reported (first satge pid)
            if(WIFEXITED(status)){
                printf("%s with pid %d exited normally\n", j->cmdline,(int)pid);
            }
            else if(WIFSIGNALED(status)){
                printf("%s with pid %d exited abnormally\n", j->cmdline, (int)pid);
            }
fflush(stdout);
j->active=false;
        }
    }
    errno=saved_errno;
}

void jobs_init(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigchild_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART; // let fgets() resume after the handler runs
    sigaction(SIGCHLD, &sa, NULL);
}

int jobs_add(const pid_t *pids, int npids, const char *cmdline) {
    if (njobs >= max_jobs || npids <= 0) {
        return -1;
    }
    Job *j = &jobs[njobs++];
    j->job_number = next_job_number++;
    j->npids = (npids > max_jobs_pids) ? max_jobs_pids : npids;
    for (int i = 0; i < j->npids; i++) {
        j->pids[i] = pids[i];
        j->pid_done[i] = false;
    }
    snprintf(j->cmdline, sizeof(j->cmdline), "%s", cmdline);
    j->active = true;
    return j->job_number;
}

void jobs_block_sigchild(sigset_t *old_mask){
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set,SIGCHLD);
    sigprocmask(SIG_BLOCK,&set,old_mask);
}

void jobs_unblock_sigchild(const sigset_t *old_mask) {
    sigprocmask(SIG_SETMASK, old_mask, NULL);
}


