#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include <pwd.h>
#include "prompt.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 256
#endif

extern char shell_home[];

void init_prompt(void) {
}

void printprompt(void) {
    const char *user_name = "user";
    struct passwd *user = getpwuid(getuid());
    if (user != NULL && user->pw_name != NULL) {
        user_name = user->pw_name;
    }

    char host_name[HOST_NAME_MAX + 1];
    if (gethostname(host_name, sizeof(host_name)) == -1) {
        strcpy(host_name, "unknown");
    }
    host_name[HOST_NAME_MAX] = '\0';

    char current_dir[PATH_MAX];
    if (getcwd(current_dir, sizeof(current_dir)) == NULL) {
        strcpy(current_dir, "?");
    }

    size_t home_len = strlen(shell_home);

    printf("<%s@%s:", user_name, host_name);
    if (home_len > 0 && strcmp(current_dir, shell_home) == 0) {
        printf("~");
    } else if (home_len > 0 && strncmp(current_dir, shell_home, home_len) == 0
               && current_dir[home_len] == '/') {
        printf("~%s", current_dir + home_len);
    } else {
        printf("%s", current_dir);
    }
    printf("> ");
    fflush(stdout);
}