#ifndef HOP_H
#define HOP_H

#include <stdbool.h>
#include <stddef.h>
#include "command.h"

void execute_hop(Command *cmd);
bool resolve_dir_arg(const char *arg, char *out, size_t out_size);
const char *get_prev_dir(void);

#endif