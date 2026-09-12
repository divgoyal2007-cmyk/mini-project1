#include <stdio.h>
#include <string.h>
#include "input.h"

void read_input(char *line, int max_len) {
    if (fgets(line, max_len, stdin) == NULL) {
        line[0] = '\0';
    }
}