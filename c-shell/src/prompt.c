#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include <pwd.h>
#include "prompt.h"
char init_shell[PATH_MAX];

void init_prompt(){
    if(getcwd(init_shell,sizeof(init_shell))==NULL){
        perror("getcwd");
        return;
    }
//printf("%s\n",init_shell);
}
void printprompt(){
   uid_t user_id=getuid();
    struct passwd *user = getpwuid(user_id);
   if(user==NULL){
    perror("getpwuid");
    return ;
   }
 //  printf("%s\n",user->pw_name);
 char host_name[_POSIX_HOST_NAME_MAX];
if( gethostname(host_name,sizeof(host_name))==-1){
perror("gethostname");
return;
}
char current_dir[PATH_MAX];
if(getcwd(current_dir,sizeof(current_dir))==NULL){
    perror("getcwd");
    return;
}
printf("<%s@%s:", user->pw_name, host_name);
if(strcmp(current_dir,init_shell)==0){
    printf("~\n");
}
else if(strncmp(current_dir,init_shell,strlen(init_shell))==0){
    printf("~%s\n", current_dir + strlen(init_shell));
}
else{
    printf("%s",current_dir);
}
 
}
