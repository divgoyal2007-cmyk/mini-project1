#ifndef TERMINAL_H
#define TERMINAL_H
#include <sys/types.h>
void terminal_init(void);
void terminal_give_to(pid_t pgid);
void terminal_reclaim(void);
pid_t terminal_shell_pgid(void);
#endif