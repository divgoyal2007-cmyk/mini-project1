#include "reveal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

extern char shell_home[];
extern char prev_dir[];

void execute_reveal(Command* cmd){
    bool flag_a=false; //hidden files
    bool flag_t=false; //recurisve tree 
    char target_path[1030]=".";
    int path_count=0;

    for(int i=1;i<cmd->argc;i++){
        char* arg=cmd->argv[i];
        if(arg[0]=='-' && strlen(arg) > 1){
            for(int j=1;j<strlen(arg);j++){
                if(arg[j]=='a'){
                    flag_a=true;
                }
                else if(arg[j]=='t'){
                    flag_t=true;
                }
                else{
                    printf("reveal : invalid syntax\n");
                    return;
                }
            }
        }
        else {
            path_count++;
            if(path_count > 1){
                printf("reval : invalid syntax\n");
                return;
            }
            strcpy(target_path,arg);

        }
    }
    char resolved_path[1030];
    if(strcmp(target_path,"~")==0){
        strcpy(resolved_path,shell_home);
    }
    else if(strcmp(target_path,"-")==0){
        if(strlen(prev_dir)==0){
            printf("reveal : no such directory\n");
            return;
        }
        strcpy(resolved_path,prev_dir);
    }
    else{
        strcpy(resolved_path,target_path);
    }
    printf("Debug->target: %s| Flag -a: %d | Flag -t %d\n",resolved_path,flag_a,flag_t);

}