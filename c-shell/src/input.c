#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "input.h"

void read_input(char* line,int size){
if(fgets(line,size,stdin)==NULL){
    printf("\n");
    exit(0);
}
int len=strlen(line);
if(line[len-1]=='\n' && len>0){
    line[len-1]='\0';
}
}
