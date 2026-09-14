#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "hop.h"
#include "prompt.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define HOP_MAX_ENTRIES 256
#define HOP_AGE_LIMIT   1000.0
#define HOP_DECAY       0.9
#define HOP_DROP_BELOW  1.0

static char prev_dir[PATH_MAX] = "";

const char *get_prev_dir(void) {
    return prev_dir;
}

typedef struct {
    char   path[PATH_MAX];
    double rank;
    long   last_access;
} hop_entry;

static hop_entry entries[HOP_MAX_ENTRIES];
static size_t    entry_count = 0;
static bool      history_loaded = false;

// ~ must mean the SHELL's own launch directory (per spec), not the OS's
// real $HOME -- those are different concepts and this implementation was
// conflating them.
static const char *get_home_dir(void) {
    extern char shell_home[];
    return shell_home;
}

static void history_file_path(char *buf, size_t size) {
    // history persistence deliberately DOES use the real $HOME, so it
    // survives across shell launches from different directories
    const char *base = getenv("HOME");
    if (base == NULL || base[0] == '\0') {
        base = get_home_dir();
    }
    snprintf(buf, size, "%s/.cshell_hop_history", base);
}

static void load_history(void) {
    if (history_loaded) {
        return;
    }
    history_loaded = true;

    char file[PATH_MAX];
    history_file_path(file, sizeof(file));

    FILE *f = fopen(file, "r");
    if (f == NULL) {
        return;
    }

    char line[PATH_MAX + 64];

    while (entry_count < HOP_MAX_ENTRIES && fgets(line, sizeof(line), f) != NULL) {
        double rank;
        long   stamp;
        int    offset = 0;

        if (sscanf(line, "%lf %ld %n", &rank, &stamp, &offset) != 2) {
            continue;
        }

        char  *path = line + offset;
        size_t len  = strlen(path);

        while (len > 0 && (path[len - 1] == '\n' || path[len - 1] == '\r')) {
            path[--len] = '\0';
        }

        if (len == 0 || len >= PATH_MAX) {
            continue;
        }

        snprintf(entries[entry_count].path, PATH_MAX, "%s", path);
        entries[entry_count].rank = rank;
        entries[entry_count].last_access = stamp;
        entry_count++;
    }
    fclose(f);
}

static void save_history(void) {
    char file[PATH_MAX];
    history_file_path(file, sizeof(file));

    FILE *f = fopen(file, "w");
    if (f == NULL) {
        return;
    }

    for (size_t i = 0; i < entry_count; i++) {
        fprintf(f, "%.6f %ld %s\n", entries[i].rank, entries[i].last_access, entries[i].path);
    }
    fclose(f);
}

static void age_entries(void) {
    double total = 0.0;
    for (size_t i = 0; i < entry_count; i++) {
        total += entries[i].rank;
    }
    if (total <= HOP_AGE_LIMIT) {
        return;
    }

    size_t kept = 0;
    for (size_t i = 0; i < entry_count; i++) {
        entries[i].rank *= HOP_DECAY;
        if (entries[i].rank >= HOP_DROP_BELOW) {
            entries[kept++] = entries[i];
        }
    }
    entry_count = kept;
}

static void record_visit(const char *path) {
    load_history();
    long now = (long)time(NULL);

    for (size_t i = 0; i < entry_count; i++) {
        if (strcmp(entries[i].path, path) == 0) {
            entries[i].rank += 1.0;
            entries[i].last_access = now;
            age_entries();
            save_history();
            return;
        }
    }

    if (entry_count == HOP_MAX_ENTRIES) {
        size_t worst = 0;
        for (size_t i = 1; i < entry_count; i++) {
            if (entries[i].rank < entries[worst].rank) {
                worst = i;
            }
        }
        entries[worst] = entries[entry_count - 1];
        entry_count--;
    }

    snprintf(entries[entry_count].path, PATH_MAX, "%s", path);
    entries[entry_count].rank = 1.0;
    entries[entry_count].last_access = now;
    entry_count++;

    age_entries();
    save_history();
}

static double frecency(const hop_entry *e, long now) {
    long age = now - e->last_access;
    if (age < 3600) return e->rank * 4.0;
    if (age < 86400) return e->rank * 2.0;
    if (age < 604800) return e->rank / 2.0;
    return e->rank / 4.0;
}

static long   sort_now;
static size_t sort_order[HOP_MAX_ENTRIES];

static int compare_matches(const void *a, const void *b) {
    size_t ia = *(const size_t *)a;
    size_t ib = *(const size_t *)b;
    double sa = frecency(&entries[ia], sort_now);
    double sb = frecency(&entries[ib], sort_now);

    if (sa > sb) return -1;
    if (sa < sb) return 1;
    return strcmp(entries[ia].path, entries[ib].path);
}

static bool is_directory(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        return false;
    }
    return S_ISDIR(st.st_mode);
}

static bool frecency_hop(const char *name) {
    load_history();
    sort_now = (long)time(NULL);
    size_t match_count = 0;

    for (size_t i = 0; i < entry_count; i++) {
        if (strstr(entries[i].path, name) != NULL) {
            sort_order[match_count++] = i;
        }
    }

    if (match_count == 0) {
        return false;
    }

    qsort(sort_order, match_count, sizeof(sort_order[0]), compare_matches);

    for (size_t i = 0; i < match_count; i++) {
        const char *candidate = entries[sort_order[i]].path;
        if (is_directory(candidate) && chdir(candidate) == 0) {
            return true;
        }
    }
    return false;
}

bool resolve_dir_arg(const char *arg, char *out, size_t out_size) {
    const char *home = get_home_dir();

    if (strcmp(arg, "~") == 0) {
        snprintf(out, out_size, "%s", home);
        return true;
    }

    if (strncmp(arg, "~/", 2) == 0) {
        snprintf(out, out_size, "%s/%s", home, arg + 2);
        return true;
    }

    if (strcmp(arg, "-") == 0) {
        if (prev_dir[0] == '\0') {
            return false;
        }
        snprintf(out, out_size, "%s", prev_dir);
        return true;
    }

    snprintf(out, out_size, "%s", arg);
    return true;
}

static void hop_one(const char *arg) {
    char target[PATH_MAX * 2];

    if (!resolve_dir_arg(arg, target, sizeof(target))) {
        printf("hop: no such directory\n");
        return;
    }

    bool allow_frecency = (strcmp(arg, "~") != 0 &&
                           strcmp(arg, "-") != 0 &&
                           strcmp(arg, ".") != 0 &&
                           strcmp(arg, "..") != 0 &&
                           strncmp(arg, "~/", 2) != 0);

    char before[PATH_MAX];
    if (getcwd(before, sizeof(before)) == NULL) {
        before[0] = '\0';
    }

    bool moved = (chdir(target) == 0);

    if (!moved && allow_frecency) {
        moved = frecency_hop(arg);
    }

    if (!moved) {
        printf("hop: no such directory\n");
        return;
    }

    char after[PATH_MAX];
    if (getcwd(after, sizeof(after)) == NULL) {
        after[0] = '\0';
    }

    if (before[0] != '\0' && strcmp(before, after) != 0) {
        snprintf(prev_dir, sizeof(prev_dir), "%s", before);
    }

    if (after[0] != '\0') {
        record_visit(after);
    }
}

void execute_hop(Command *cmd) {
    if (cmd->argc <= 1) {
        hop_one("~");
        return;
    }
    for (int i = 1; i < cmd->argc; i++) {
        hop_one(cmd->argv[i]);
    }
}