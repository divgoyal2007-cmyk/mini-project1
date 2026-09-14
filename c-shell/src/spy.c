#include "spy.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef __linux__
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <limits.h>

// snprintf into a PATH_MAX-sized buffer with a short fixed suffix can
// never actually truncate here (proc paths are always well under
// PATH_MAX), but gcc's static checker can't prove that -- silence just
// this warning for these specific, known-safe concatenations.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"

static const char* get_file_type(mode_t mode) {
    if (S_ISDIR(mode)) return "DIR";
    if (S_ISREG(mode)) return "REG";
    if (S_ISCHR(mode)) return "CHR";
    if (S_ISBLK(mode)) return "BLK";
    if (S_ISFIFO(mode)) return "FIFO";
    if (S_ISLNK(mode)) return "LNK";
    if (S_ISSOCK(mode)) return "SOCK";
    return "UNKNOWN";
}

// stat_path: what actually gets stat()'d for TYPE. For fd symlinks this
// MUST be the /proc/pid/fd/N path itself -- the kernel resolves type info
// correctly there even for pipes/sockets, but their readlink() text
// (e.g. "pipe:[123]") is not a real statable path and would just fail.
// display_path: what gets printed in the PATH column.
static void print_entry(const char* pid, const char* fd, const char* stat_path,
                         const char* display_path) {
    struct stat st;
    if (stat(stat_path, &st) == 0) {
        printf("%-6s %-4s %-6s %s\n", pid, fd, get_file_type(st.st_mode), display_path);
    }
}

void execute_spy(Command *cmd) {
    if (cmd->argc > 2) {
        printf("spy: invalid syntax\n");
        return;
    }

    char pid_str[32];
    if (cmd->argc == 2) {
        snprintf(pid_str, sizeof(pid_str), "%s", cmd->argv[1]);   // fixed: always null-terminated
    } else {
        snprintf(pid_str, sizeof(pid_str), "%d", getpid());
    }

    char proc_path[PATH_MAX];
    snprintf(proc_path, sizeof(proc_path), "/proc/%s", pid_str);

    struct stat st;
    if (stat(proc_path, &st) != 0) {
        printf("spy: no such process\n");
        return;
    }

    printf("%-6s %-4s %-6s %s\n", "PID", "FD", "TYPE", "PATH");

    char link_target[PATH_MAX];
    char target_path[PATH_MAX + 32];
    ssize_t len;

    // cwd/exe always point to real files, so readlink's own text works
    // fine as both the stat target and the display path.
    snprintf(target_path, sizeof(target_path), "%s/cwd", proc_path);
    if ((len = readlink(target_path, link_target, sizeof(link_target) - 1)) != -1) {
        link_target[len] = '\0';
        print_entry(pid_str, "cwd", link_target, link_target);
    }

    snprintf(target_path, sizeof(target_path), "%s/exe", proc_path);
    if ((len = readlink(target_path, link_target, sizeof(link_target) - 1)) != -1) {
        link_target[len] = '\0';
        print_entry(pid_str, "txt", link_target, link_target);
    }

    // /proc/pid/maps lines already contain real absolute paths directly,
    // no readlink needed here.
    snprintf(target_path, sizeof(target_path), "%s/maps", proc_path);
    FILE *maps = fopen(target_path, "r");
    if (maps) {
        char line[1024];
        char *seen_paths[1024];
        int seen_count = 0;

        while (fgets(line, sizeof(line), maps)) {
            char *path = strchr(line, '/');
            if (path) {
                path[strcspn(path, "\n")] = 0;
                int duplicate = 0;
                for (int i = 0; i < seen_count; i++) {
                    if (strcmp(seen_paths[i], path) == 0) {
                        duplicate = 1;
                        break;
                    }
                }
                if (!duplicate && seen_count < 1024) {
                    seen_paths[seen_count++] = strdup(path);
                    print_entry(pid_str, "mem", path, path);
                }
            }
        }
        for (int i = 0; i < seen_count; i++) free(seen_paths[i]);
        fclose(maps);
    }

    // numbered fds CAN be pipes/sockets -- must stat the fd symlink path
    // itself for TYPE, not readlink()'s text (this was the bug).
    snprintf(target_path, sizeof(target_path), "%s/fd", proc_path);
    DIR *dir = opendir(target_path);
    if (dir) {
        struct dirent *dp;
        while ((dp = readdir(dir)) != NULL) {
            if (dp->d_name[0] == '.') continue;

            char fd_path[PATH_MAX + 64];
            snprintf(fd_path, sizeof(fd_path), "%s/%s", target_path, dp->d_name);

            if ((len = readlink(fd_path, link_target, sizeof(link_target) - 1)) != -1) {
                link_target[len] = '\0';
                print_entry(pid_str, dp->d_name, fd_path, link_target);
            }
        }
        closedir(dir);
    }
}

#pragma GCC diagnostic pop

#else
// macOS fallback -- /proc doesn't exist there
void execute_spy(Command *cmd) {
    (void)cmd;
    printf("spy: this command requires Linux /proc filesystem and cannot be run on macOS.\n");
}
#endif