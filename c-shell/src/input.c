#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "input.h"

InputResult read_input(char *line, int max_len) {
    errno = 0;
    if (fgets(line, max_len, stdin) == NULL) {
        if (errno == EINTR) {
            return INPUT_INTERRUPTED;   // Ctrl-C/Ctrl-Z fired while waiting
        }
        line[0] = '\0';
        return INPUT_EOF;               // genuine Ctrl-D on an empty line
    }

    // strip trailing newline if present (a Ctrl-D-flushed partial line won't have one)
    size_t len = strlen(line);
    if (len > 0 && line[len - 1] == '\n') {
        line[len - 1] = '\0';
    }
    return INPUT_OK;
}