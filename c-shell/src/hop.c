#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "hop.h"
    extern char shell_home[];
    extern char prev_dir[];
    void execute(const char* path){
    char current_dir[1024];
    getcwd(current_dir,sizeof(current_dir));
    char target[1024];
    if(path==NULL || strcmp(path,"~")==0){
        strcpy(target,shell_home);
    }
    else if(strcmp(path,"-")==0){
        if(strlen(prev_dir)==0){
            return;
        }
        strcpy(target,prev_dir);
    }
    else{
        strcpy(target,path);
    }
    if(chdir(target)==0){
        strcpy(prev_dir,current_dir);
    }
    else{
        printf("no such directory\n");
    }

    }
