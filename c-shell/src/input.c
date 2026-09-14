#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "input.h"

InputResult read_input(char *line, int max_len) {
    errno = 0;
    if (fgets(line, max_len, stdin) == NULL) {
        InputResult result = (errno == EINTR) ? INPUT_INTERRUPTED : INPUT_EOF;
        clearerr(stdin);   // reset the stream's EOF flag, or every future
                            // read on it would immediately return EOF again
        if (result == INPUT_EOF) line[0] = '\0';
        return result;
    }

    // strip trailing newline if present (a Ctrl-D-flushed partial line won't have one)
    size_t len = strlen(line);
    if (len > 0 && line[len - 1] == '\n') {
        line[len - 1] = '\0';
    }
    return INPUT_OK;
}