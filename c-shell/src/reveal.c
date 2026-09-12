#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include "reveal.h"
#include "hop.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static int compare_names(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static bool path_is_dir(const char *path) {
    struct stat st;
    if (lstat(path, &st) != 0) {
        return false;
    }
    return S_ISDIR(st.st_mode);
}

static void reveal_directory(const char *base_path, const char *prefix, bool flag_a, bool flag_t) {
    char full_path[PATH_MAX * 2];
    if (prefix[0] == '\0') {
        snprintf(full_path, sizeof(full_path), "%s", base_path);
    } else {
        snprintf(full_path, sizeof(full_path), "%s/%s", base_path, prefix);
    }

    DIR *dir = opendir(full_path);
    if (dir == NULL) {
        if (prefix[0] == '\0') {
            printf("reveal: no such directory\n");
        }
        return;
    }

    char **entries = NULL;
    size_t count = 0, cap = 0;
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        if (!flag_a && entry->d_name[0] == '.') {
            continue;
        }
        if (count == cap) {
            cap = (cap == 0) ? 64 : cap * 2;
            char **tmp = realloc(entries, cap * sizeof(char *));
            if (tmp == NULL) {
                break;
            }
            entries = tmp;
        }
        entries[count++] = strdup(entry->d_name);
    }
    closedir(dir);

    qsort(entries, count, sizeof(char *), compare_names);

    for (size_t i = 0; i < count; i++) {
        char rel[PATH_MAX * 2];
        if (prefix[0] == '\0') {
            snprintf(rel, sizeof(rel), "%s", entries[i]);
        } else {
            snprintf(rel, sizeof(rel), "%s/%s", prefix, entries[i]);
        }

        char abs_path[PATH_MAX * 3];
        snprintf(abs_path, sizeof(abs_path), "%s/%s", base_path, rel);

        bool is_dir = path_is_dir(abs_path);

        if (flag_t && is_dir) {
            printf("%s/\n", rel);
            reveal_directory(base_path, rel, flag_a, flag_t);
        } else {
            printf("%s\n", rel);
        }
        free(entries[i]);
    }
    free(entries);
}

void execute_reveal(Command *cmd) {
    bool flag_a = false;
    bool flag_t = false;
    const char *target = ".";
    int path_count = 0;

    for (int i = 1; i < cmd->argc; i++) {
        const char *arg = cmd->argv[i];

        if (arg[0] == '-' && arg[1] != '\0') {
            for (size_t j = 1; j < strlen(arg); j++) {
                if (arg[j] == 'a') {
                    flag_a = true;
                } else if (arg[j] == 't') {
                    flag_t = true;
                } else {
                    printf("reveal: invalid syntax\n");
                    return;
                }
            }
        } else {
            path_count++;
            if (path_count > 1) {
                printf("reveal: invalid syntax\n");
                return;
            }
            target = arg;
        }
    }

    char resolved[PATH_MAX * 2];
    if (!resolve_dir_arg(target, resolved, sizeof(resolved))) {
        printf("reveal: no such directory\n");
        return;
    }

    if (!path_is_dir(resolved)) {
        printf("reveal: no such directory\n");
        return;
    }

    reveal_directory(resolved, "", flag_a, flag_t);
}