#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>
#include <unistd.h>
#include "prompt.h"
#include "input.h"
#include "lexer.h"
#include "parser.h"
#include "command.h"
#include "sys_command.h"
#include "jobs.h"
#include "terminal.h"
#include "hop.h"
#include "reveal.h"
#include "peek.h"
#include "locate.h"
#include "activities.h"
#include "resume.h"
#include "ping.h"
#include "spy.h"
#include "snoop.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define MAX_INPUT_LEN 1030

char shell_home[PATH_MAX];
char prev_dir[PATH_MAX];


static void shell_exit(void) {
    jobs_hangup_all();
    exit(0);
}

// runs one ';'/'&'-separated group
static void run_group(Pipeline *p, bool *stop_sequence) {
    Command *cmd = p->stages[0];

    // builtins: single command, not backgrounded
    if (p->nstages == 1 && !p->background) {
        if (strcmp(cmd->name, "exit") == 0) {
            shell_exit();
        }
        if (strcmp(cmd->name, "hop") == 0)        { execute_hop(cmd);        return; }
        if (strcmp(cmd->name, "reveal") == 0)     { execute_reveal(cmd);     return; }
        if (strcmp(cmd->name, "peek") == 0)       { execute_peek(cmd);       return; }
        if (strcmp(cmd->name, "locate") == 0)     { execute_locate(cmd);     return; }
        if (strcmp(cmd->name, "activities") == 0) { execute_activities(cmd); return; }
        if (strcmp(cmd->name, "resume") == 0)     { execute_resume(cmd);     return; }
        if (strcmp(cmd->name, "ping") == 0)       { execute_ping(cmd);       return; }
        if (strcmp(cmd->name, "spy") == 0)        { execute_spy(cmd);        return; }
        if (strcmp(cmd->name, "snoop") == 0)      { execute_snoop(cmd);      return; }
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
    jobs_init();       // install SIGCHLD handler
    terminal_init();   // claim terminal, install SIGINT/SIGTSTP handlers

    char line[MAX_INPUT_LEN];
    bool eof_warned = false;   // tracks the Ctrl-D "press again" state

    while (1) {
        printprompt();
        InputResult r = read_input(line, MAX_INPUT_LEN);

        if (r == INPUT_INTERRUPTED) {
            continue;   // handler already printed a newline, just redraw
        }

        if (r == INPUT_EOF) {
            if (eof_warned) {
                shell_exit();   // second Ctrl-D in a row -> exit anyway
            }
            if (jobs_any_stopped()) {
                printf("\ncshell: there are stopped jobs\n");   // E2 #7
                eof_warned = true;
                continue;
            }
            shell_exit();   // plain EOF, nothing stopped -> exit
        }

        eof_warned = false;   // got real input -> reset the two-strike counter

        Tokenlist *list = token(line);
        if (list == NULL) continue;

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