#ifndef jobs_h
#define jobs_h

#include <sys/types.h>
#include <signal.h>
#include <stdbool.h>

#define max_jobs 256
#define max_jobs_pids 64

//installs the sigchild handler
void jobs_init(void);

// pids holds every process
//display name(just the first stages command name)
//returns the assigned job number or -1 on failure 
int jobs_add(const pid_t *pids, int npids, const char *cmdline);


void jobs_block_sigchild(sigset_t *oldmask);
void jobs_unblock_sigchild(const sigset_t *old_mask);

#endif

