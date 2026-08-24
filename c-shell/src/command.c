#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "command.h"

Command* extract_command(Tokenlist* list){
    Command* cmd=malloc(sizeof(Command));
    cmd->name=NULL;
    cmd->argc=0;
    cmd->input_file=NULL;
    cmd->output_file=NULL;
    cmd->append_output=false;
    cmd->background=false;

    for(int i=0;i<512;i++){
        cmd->argv[i]=NULL;
    }
    if(list==NULL || list->size==0)
    {
        return cmd;
    }
    for(int i=0;i<list->size;i++){
        Tokentype type=list->tokens[i].type;
        char* val=list->tokens[i].value;
        if(type==token_eof)break;
        if(type==token_word){
            if(cmd->name==NULL){
                cmd->name=strdup(val);
            }
            cmd->argv[cmd->argc]=strdup(val);
            cmd->argc++;
        }
        else if (type==token_op_lt){
cmd->input_file = strdup(list->tokens[i+1].value);
            i++;
        }
        else if (type == token_op_gt) {
            cmd->output_file = strdup(list->tokens[i+1].value);
            cmd->append_output = false;
            i++; 
        }
     else if (type == token_op_gtgt) {
            cmd->output_file = strdup(list->tokens[i+1].value);
            cmd->append_output = true;
            i++; 
        }

        else if (type == token_op_amp) {
            cmd->background = true;
        }
    }
    return cmd;
}

void free_command(Command* cmd){
    if(cmd==NULL)return;
    if(cmd->name){
        free(cmd->name);
    }
    if(cmd->input_file){
        free(cmd->input_file);
    }
    if(cmd->output_file){
        free(cmd->output_file);
    }
    for(int i=0;i<cmd->argc;i++){
        if(cmd->argv[i]){
            free(cmd->argv[i]);
        }
    }
    free(cmd);
}
