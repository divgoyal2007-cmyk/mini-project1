#ifndef SYS_COMMAND_H
#define SYS_COMMAND_H

#include <stdbool.h>
#include "command.h"

bool execute_pipeline_fg(Pipeline *p);
 
// Launches every stage of the pipeline without waiting, registers it as
// a background job, and immediately prints "[job_number] pid" (pid of
// the first stage), per spec.
void execute_pipeline_bg(Pipeline *p);

#endif