#include "prompt.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "input.h"
#include "lexer.h"
#include "parser.h"
#include "command.h"
#include "reveal.h"
#include "hop.h"
#define MAX 1030
char shell_home[MAX];
char prev_dir[MAX];
int main(void)
{
    if (getcwd(shell_home, sizeof(shell_home)) == NULL) {
        perror("getcwd error");
        return 1;
    }
    prev_dir[0] = '\0';
    init_prompt();
    char line[MAX];
    while(1){
printprompt();
read_input(line,MAX);
//printf("%s\n",line);
Tokenlist* list=token(line);
if(validate(list)==true){
Command* cmd=extract_command(list);
if(cmd->name!=NULL){
    if(strcmp(cmd->name,"hop")==0){
        if(cmd->argc==1){
            execute(NULL);
        }
    
    else { 
       for (int i = 1; i < cmd->argc; i++) {
               execute(cmd->argv[i]);
                        }
                    }
                }
    else if(strcmp(cmd->name,"reveal")==0){
        execute_reveal(cmd);
    }
    else if (strcmp(cmd->name, "exit") == 0) {
                    free_command(cmd); // Clean up memory
                    free_token(list);
                    exit(0);           // Break the loop and close the shell!
                }
}
free_command(cmd);
}
else{
    printf("chsell : invalid syntax\n");
}
// printf("token type: %d, value: %s\n",list->tokens[i].type, list->tokens[i].value);

free_token(list);
    }
    return 0;
}