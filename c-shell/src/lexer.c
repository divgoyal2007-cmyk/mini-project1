#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"

void add_token(Tokenlist* list,Tokentype type,const char* val){
    if(list->size >= list->capacity){
        if(list->capacity==0){
            list->capacity=16;
        }
        else{
            list->capacity=list->capacity * 2;
        }
        list->tokens=realloc(list->tokens,list->capacity*sizeof(Token));
    }
    list->tokens[list->size].type=type;
    if(val!=NULL){
        list->tokens[list->size].value=strdup(val);
    }
    else{
        list->tokens[list->size].value=NULL;
    }
    list->size++;
}

void free_token(Tokenlist* list){
    if(list==NULL)return;
    for(int i=0;i<list->size;i++){
        if(list->tokens[i].value !=NULL){
            free(list->tokens[i].value);
        }
    }
    free(list->tokens);
    free(list);
}

Tokenlist* token(char* input){
    Tokenlist* list=malloc(sizeof(Tokenlist));
    list->tokens=NULL;
    list->size=0;
    list->capacity=0;
    int k=0;
    int len=strlen(input);
    while(k < len){
        if(input[k]=='\t' || input[k]==' ' || input[k]=='\n'){
            k++;
            continue;
        }
        if(input[k]=='<'){
add_token(list,token_op_lt,"<");
k++;
continue;
        }
      if(input[k]=='|'){
add_token(list,token_op_pipe,"|");
k++;
continue;
        }
          if(input[k]=='&'){
add_token(list,token_op_amp,"&");
k++;
continue;
        }
  if(input[k]==';'){
add_token(list,token_op_semi,";");
k++;
continue;
        }
        if(input[k]=='>'){
            if(k+1 < len && input[k+1]=='>'){
                add_token(list,token_op_gtgt,">>");
            }
            else{
                add_token(list,token_op_gt,">");
                k++;
            }
            continue;
        }
        char temp[1030];
        int temp_index=0;
        while(k < len && input[k]!=' ' && input[k]!='\t' && input[k]!='\n' && input[k]!='\r' && input[k]!='>' && input[k]!='<' && input[k]!='|' && input[k]!='&' && input[k]!=';') {
            if(input[k]=='\\'){
                k++;
                if(k>=len){
add_token(list,token_error,"trailingslash");
return list;
                }
                temp[temp_index++]=input[k++];
            }
            else if(input[k]=='\''){
                k++;
                while(k<len && input[k]!='\''){
                    temp[temp_index]=input[k++];
                }
                if(k>=len){
                    add_token(list,token_error,"unclosed_quote");
                    return list;
                }
                k++;
            }
            else if(input[k]=='"'){
k++;
while(k< len && input[k]!='"'){
    if(input[k]=='\\'){
        if(k+1 < len && (input[k+1]=='"' || input[k+1]=='\\')){
k++;
temp[temp_index++]=input[k++];
        }
        else if(k+1 < len){
            temp[temp_index++]=input[k++];
            temp[temp_index++]=input[k++];
        }
        else{
            add_token(list,token_error,"trailingslash");
            return list;
        }
    }
    else{
       temp[temp_index++]=input[k++];
    }
}
if(k >=len){
    add_token(list,token_error,"unclosed_quote");
    return list;
}
k++;
            }
    else{
          temp[temp_index++]=input[k++];  
            }
        }
        temp[temp_index]='\0';
        add_token(list,token_word,temp);
    }
    add_token(list,token_eof,"EOF");
    return list;
}