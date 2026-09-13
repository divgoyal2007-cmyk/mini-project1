#include "activities.h"
#include "jobs.h"
#include <stdio.h>

void execute_activities(Command *cmd){
    (void)cmd;
    int total=jobs_total_slots();
    for(int i=0;i<total;i++){
        Job *j=jobs_get(i);
        if(j==NULL || !j->active)continue;
          printf("[%d] pgid %d\n", j->job_number, (int)j->pgid);
 
        for (int k = 0; k < j->npids; k++) {
            if (j->pid_done[k]) continue;   // that specific stage already exited
            const char *state = (j->state == JOB_STOPPED) ? "Stopped" : "Running";
            printf("  %d %s %s\n", (int)j->pids[k], j->stage_names[k], state);
        }
    }
}