#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include "prompt.h"
#include "input.h"
#include "lexer.h"
#include "parser.h"
#include "command.h"
#include "sys_command.h"
#include "jobs.h"
#include "hop.h"
#include "reveal.h"
#include "peek.h"
#include "locate.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define MAX_INPUT_LEN 1030

char shell_home[PATH_MAX];
char prev_dir[PATH_MAX];

// Runs one ';'/'&'-separated group. Sets *stop_sequence to true if this
// group was a lone (unpiped) command that failed to execute -- per D1,
// that's the only case that halts the rest of the ';' chain.
static void run_group(Pipeline *p, bool *stop_sequence) {
    Command *cmd = p->stages[0];

  
    if (p->nstages == 1 && !p->background) {
        if (strcmp(cmd->name, "exit") == 0) {
            exit(0);
        }
        if (strcmp(cmd->name, "hop") == 0)    { execute_hop(cmd);    return; }
        if (strcmp(cmd->name, "reveal") == 0) { execute_reveal(cmd); return; }
        if (strcmp(cmd->name, "peek") == 0)   { execute_peek(cmd);   return; }
        if (strcmp(cmd->name, "locate") == 0) { execute_locate(cmd); return; }
    }

    if (p->background) {
        execute_pipeline_bg(p);
    } else {
        if (execute_pipeline_fg(p)) {
            *stop_sequence = true;
        }
    }
}

int main(void) {
    if (getcwd(shell_home, sizeof(shell_home)) == NULL) {
        perror("getcwd");
        return 1;
    }
    prev_dir[0] = '\0';
    init_prompt();
    jobs_init();

    char line[MAX_INPUT_LEN];

    while (1) {
        printprompt();
        read_input(line, MAX_INPUT_LEN);

        Tokenlist *list = token(line);
        if (list == NULL) {
            continue;
        }

        if (!validate(list)) {
            printf("cshell: invalid syntax\n");
            free_token(list);
            continue;
        }

        CommandLine *cl = extract_line(list);
        if (cl != NULL) {
            bool stop_sequence = false;
            for (int i = 0; i < cl->ngroups && !stop_sequence; i++) {
                run_group(cl->groups[i], &stop_sequence);
            }
            free_command_line(cl);
        }

        free_token(list);
    }
    return 0;
}