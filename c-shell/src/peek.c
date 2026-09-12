#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include "peek.h"

#define CHUNK 4096

static bool is_empty_line(const char *line) {
    for (size_t i = 0; line[i] != '\0'; i++) {
        if (line[i] != ' ' && line[i] != '\t' && line[i] != '\n' && line[i] != '\r') {
            return false;
        }
    }
    return true;
}

static void peek_forward(FILE *fp, bool flag_n, int *num) {
    char  *line = NULL;
    size_t cap = 0;
    ssize_t n;

    while ((n = getline(&line, &cap, fp)) > 0) {
        if (flag_n && !is_empty_line(line)) {
            printf("%d %s", (*num)++, line);
        } else {
            fputs(line, stdout);
        }
    }
    free(line);
}

static void emit_reversed(char *buf, size_t len, bool flag_n, int *num) {
    for (size_t i = 0; i < len / 2; i++) {
        char t = buf[i];
        buf[i] = buf[len - 1 - i];
        buf[len - 1 - i] = t;
    }
    buf[len] = '\0';

    if (flag_n && !is_empty_line(buf)) {
        printf("%d %s\n", (*num)--, buf);
    } else {
        printf("%s\n", buf);
    }
}

static int count_nonempty_lines(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        return 0;
    }
    char  *line = NULL;
    size_t cap = 0;
    int    total = 0;

    while (getline(&line, &cap, fp) > 0) {
        if (!is_empty_line(line)) {
            total++;
        }
    }
    free(line);
    fclose(fp);
    return total;
}

static void peek_reverse_file(const char *filename, bool flag_n, int *num) {
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        printf("peek: no such file or directory\n");
        return;
    }

    off_t size = lseek(fd, 0, SEEK_END);
    if (size <= 0) {
        close(fd);
        return;
    }

    int number = *num;
    if (flag_n) {
        int total = count_nonempty_lines(filename);
        number = *num + total - 1;
        *num += total;
    }

    size_t cap = CHUNK;
    size_t len = 0;
    char  *buf = malloc(cap);
    if (buf == NULL) {
        close(fd);
        return;
    }

    char  chunk[CHUNK];
    off_t pos = size;

    while (pos > 0) {
        off_t want = (pos > (off_t)CHUNK) ? (off_t)CHUNK : pos;
        pos -= want;
        if (lseek(fd, pos, SEEK_SET) < 0) {
            break;
        }

        ssize_t got = 0;
        while (got < (ssize_t)want) {
            ssize_t r = read(fd, chunk + got, (size_t)want - (size_t)got);
            if (r <= 0) {
                break;
            }
            got += r;
        }

        for (ssize_t i = got - 1; i >= 0; i--) {
            char c = chunk[i];

            if (c == '\n') {
                if (pos + i == size - 1) {
                    continue;
                }
                emit_reversed(buf, len, flag_n, &number);
                len = 0;
            } else {
                if (len + 2 >= cap) {
                    cap *= 2;
                    char *tmp = realloc(buf, cap);
                    if (tmp == NULL) {
                        goto done;
                    }
                    buf = tmp;
                }
                buf[len++] = c;
            }
        }
    }

    emit_reversed(buf, len, flag_n, &number);

done:
    free(buf);
    close(fd);
}

static void peek_reverse_stream(FILE *fp, bool flag_n, int *num) {
    char  **lines = NULL;
    size_t  count = 0, cap = 0;
    char   *line = NULL;
    size_t  lcap = 0;

    while (getline(&line, &lcap, fp) > 0) {
        if (count == cap) {
            cap = (cap == 0) ? 128 : cap * 2;
            char **tmp = realloc(lines, cap * sizeof(char *));
            if (tmp == NULL) {
                break;
            }
            lines = tmp;
        }
        lines[count++] = strdup(line);
    }
    free(line);

    int number = *num;
    if (flag_n) {
        int total = 0;
        for (size_t i = 0; i < count; i++) {
            if (!is_empty_line(lines[i])) {
                total++;
            }
        }
        number = *num + total - 1;
        *num += total;
    }

    for (size_t i = count; i-- > 0; ) {
        size_t l = strlen(lines[i]);
        if (l > 0 && lines[i][l - 1] == '\n') {
            lines[i][l - 1] = '\0';
        }
        if (flag_n && !is_empty_line(lines[i])) {
            printf("%d %s\n", number--, lines[i]);
        } else {
            printf("%s\n", lines[i]);
        }
        free(lines[i]);
    }
    free(lines);
}

static void peek_file(const char *filename, bool flag_n, bool flag_r, int *num) {
    bool use_stdin = (filename == NULL || strcmp(filename, "-") == 0);

    if (use_stdin) {
        int fd = dup(STDIN_FILENO);
        if (fd < 0) {
            return;
        }
        FILE *in = fdopen(fd, "r");
        if (in == NULL) {
            close(fd);
            return;
        }
        if (flag_r) {
            peek_reverse_stream(in, flag_n, num);
        } else {
            peek_forward(in, flag_n, num);
        }
        fclose(in);
        return;
    }

    struct stat st;
    if (stat(filename, &st) != 0) {
        printf("peek: no such file or directory\n");
        return;
    }
    if (S_ISDIR(st.st_mode)) {
        printf("peek: is a directory\n");
        return;
    }

    if (flag_r) {
        peek_reverse_file(filename, flag_n, num);
        return;
    }

    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        printf("peek: no such file or directory\n");
        return;
    }
    peek_forward(fp, flag_n, num);
    fclose(fp);
}

void execute_peek(Command *cmd) {
    bool flag_n = false;
    bool flag_r = false;
    char *files[MAX_ARGS];
    int   file_count = 0;

    for (int i = 1; i < cmd->argc; i++) {
        char *arg = cmd->argv[i];

        if (arg[0] == '-' && arg[1] != '\0') {
            for (size_t j = 1; j < strlen(arg); j++) {
                if (arg[j] == 'n') {
                    flag_n = true;
                } else if (arg[j] == 'r') {
                    flag_r = true;
                } else {
                    printf("peek: invalid syntax\n");
                    return;
                }
            }
        } else {
            files[file_count++] = arg;
        }
    }

    int num = 1;

    if (file_count == 0) {
        peek_file(NULL, flag_n, flag_r, &num);
    } else {
        for (int i = 0; i < file_count; i++) {
            peek_file(files[i], flag_n, flag_r, &num);
        }
    }
    fflush(stdout);
}