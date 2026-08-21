#include "prompt.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "input.h"
#define MAX 1030
int main(void)
{
    init_prompt();
    char line[MAX];
    while(1){
printprompt();
read_input(line,MAX);
//printf("%s\n",line);
    }
    return 0;
}