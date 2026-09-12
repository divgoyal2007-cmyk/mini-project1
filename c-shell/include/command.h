#ifndef COMMAND_H
#define COMMAND_H

#include <stdbool.h>
#include "lexer.h"

#define MAX_ARGS   512
#define MAX_REDIR  32
#define MAX_STAGES 64
#define MAX_GROUPS 64

typedef struct {
    char *name;
    char *argv[MAX_ARGS];
    int  argc;
    char *in_files[MAX_REDIR];
    int  in_count;
    char *out_files[MAX_REDIR];
    bool out_append[MAX_REDIR];
    int  out_count;
} Command;

typedef struct {
    Command *stages[MAX_STAGES];
    int      nstages;
    bool     background;
} Pipeline;

typedef struct {
    Pipeline *groups[MAX_GROUPS];
    int       ngroups;
} CommandLine;
 
CommandLine *extract_line(Tokenlist *list);
void free_command_line(CommandLine *cl);
#endif