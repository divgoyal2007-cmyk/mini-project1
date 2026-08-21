#include "prompt.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "input.h"
#include "lexer.h"
#define MAX 1030
int main(void)
{
    init_prompt();
    char line[MAX];
    while(1){
printprompt();
read_input(line,MAX);
//printf("%s\n",line);
Tokenlist* list=token(line);
for(int i=0;i< list->size ;i++){
    if(list->tokens[i].type == token_error){
        printf("cshell: syntax invalid\n");
        break;
    }
    printf("token type: %d, value: %s\n",list->tokens[i].type, list->tokens[i].value);
}
free_token_list(list);
    }
    return 0;
}