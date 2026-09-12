#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>
#include <unistd.h>
#include <sys/stat.h>
#include "locate.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static bool is_executable(const char *filepath) {
    struct stat st;
    return (stat(filepath, &st) == 0 &&
            S_ISREG(st.st_mode) &&
            access(filepath, X_OK) == 0);
}

void execute_locate(Command *cmd) {
    if (cmd->argc < 2) {
        printf("locate: invalid syntax\n");
        return;
    }

    for (int i = 1; i < cmd->argc; i++) {
        const char *target = cmd->argv[i];
        bool found = false;

        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            char check[PATH_MAX * 2];
            snprintf(check, sizeof(check), "%s/%s", cwd, target);
            if (is_executable(check)) {
                printf("%s\n", check);
                found = true;
            }
        }

        const char *path_env = getenv("PATH");
        if (path_env != NULL) {
            char *copy = strdup(path_env);
            if (copy != NULL) {
                char *saveptr = NULL;
                for (char *dir = strtok_r(copy, ":", &saveptr);
                     dir != NULL;
                     dir = strtok_r(NULL, ":", &saveptr)) {
                    char check[PATH_MAX * 2];
                    snprintf(check, sizeof(check), "%s/%s", dir, target);
                    if (is_executable(check)) {
                        printf("%s\n", check);
                        found = true;
                    }
                }
                free(copy);
            }
        }

        if (!found) {
            printf("locate: command not found (%s)\n", target);
        }
    }
    fflush(stdout);
}