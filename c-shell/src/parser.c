#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "parser.h"
#include <stdbool.h>

bool parse_arg(Tokenlist* list,int* index);
bool parse_cmd(Tokenlist* list,int* index);
bool parse_target(Tokenlist* list,int* index);
bool parse_background(Tokenlist* list,int* index);

bool parse_line(Tokenlist* list,int* index){
    if(list->tokens[*index].type==token_eof || *index >= list->size){
        return true;
    }
    if(list->tokens[*index].type==token_word){
        (*index)++;
        return parse_arg(list,index);
    }
    else{
        return false;
    }
}

bool parse_arg(Tokenlist* list,int* index){
    if(list->tokens[*index].type==token_eof){
        return true;
    }
    if(list->tokens[*index].type==token_word){
         (*index)++;
        return parse_arg(list,index);
    }
    if(list->tokens[*index].type== token_op_gt || list->tokens[*index].type == token_op_lt || list->tokens[*index].type == token_op_gtgt){
         (*index)++;
        return parse_target(list,index);
    }
    if(list->tokens[*index].type== token_op_pipe || list->tokens[*index].type == token_op_semi){
        (*index)++;
        return parse_cmd(list,index);
    }
    if(list->tokens[*index].type == token_op_amp){
         (*index)++;
        return parse_background(list,index);
    }
    else{
        return false;
    }
}
bool parse_cmd(Tokenlist* list,int* index){
if(list->tokens[*index].type==token_eof){
    return false;
}
if(list->tokens[*index].type==token_word){
         (*index)++;
        return parse_arg(list,index);
    }
else{
        return false;
    }
}

bool parse_target(Tokenlist* list,int* index){
    if(list->tokens[*index].type==token_eof){
    return false;
}
if(list->tokens[*index].type==token_word){
         (*index)++;
        return parse_arg(list,index);
    }
else{
        return false;
    }
}
bool parse_background(Tokenlist* list,int* index){
    if(list->tokens[*index].type==token_eof || *index >= list->size){
        return true;
    }
    if(list->tokens[*index].type==token_word){
         (*index)++;
        return parse_arg(list,index);
    }
    else{
        return false;
    }
}
bool validate(Tokenlist* list){
    if(list==NULL || list->size==0)return false;
    for(int i=0;i<list->size;i++){
        if(list->tokens[i].type==token_error){
            return false;
            break;
        }
    }
    int index=0;
    bool temp=parse_line(list,&index);
    if(temp==true){
        if(list->tokens[index].type==token_eof){
return true;
        }
        else{
            return false;
        }
    }
    else{
        return false;
    }
}