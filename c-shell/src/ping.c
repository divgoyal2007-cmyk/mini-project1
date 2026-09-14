#include "ping.h"
#include "jobs.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <stdbool.h>

static bool is_nonneg_int(const char *s) {
    if (s == NULL || *s == '\0') return false;
    for (const char *p = s; *p; p++) {
        if (*p < '0' || *p > '9') return false;
    }
    return true;
}

void execute_ping(Command* cmd){
    if(cmd->argc!=3){
        printf("ping: invalid syntax\n");
        return;
    }
    const char *target_str=cmd->argv[1];
    const char *sig_str=cmd->argv[2];
    if(!is_nonneg_int(sig_str)){
        printf("ping: invalid syntax\n");
        return;
    }
    long typed_signal=strtol(sig_str,NULL,10);
    int actual_signal=(int)(typed_signal % 64);
    bool is_job=(target_str[0]=='%');
    const char *num_part=is_job ? target_str+1 : target_str;
    if (!is_nonneg_int(num_part)) {
        printf("ping: no such process found\n");   // malformed target -> unknown
        return;
    }
    long target_num=strtol(num_part,NULL,10);
    if(is_job)
    {
        Job* j=jobs_find_by_number((int)target_num);
        if(j==NULL){
            printf("ping: no such process found\n");
            return;
        }
        if (kill(-j->pgid, actual_signal) < 0) {
            printf("ping: no such process found\n");
            return;
        }
    }
    else{
        pid_t pid=(pid_t)target_num;
        Job* j=jobs_find_by_pid(pid);
        if(j==NULL){
            printf("ping: no such process found\n");
            return;
        }
        if(kill(pid,actual_signal)< 0){
            printf("ping: no such process found\n");
            return;
        }
    }
    printf("Sent signal %ld to %s\n",typed_signal,target_str);

}