#ifndef command_h
#define command_h
#include <stdbool.h>
#include "lexer.h"
typedef struct{
    char* name;
    char *argv[512];     
    int argc;            // Number of arguments
    char *input_file;    // Target file if '<' was used
    char *output_file;   // Target file if '>' or '>>' was used
    bool append_output;  // True if '>>', False if '>'
    bool background;     // True if '&' was used
} Command;

Command* extract_command(Tokenlist* list);
void free_command(Command* cmd);
#endif