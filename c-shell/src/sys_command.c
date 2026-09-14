#include "sys_command.h"
#include "jobs.h"
#include "terminal.h"
#include "hop.h"
#include "reveal.h"
#include "peek.h"
#include "locate.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>

#define NOT_FOUND_EXIT 127   // sentinel exit code meaning "couldn't exec"

// close every pipe fd -- used by parent (all of them) and children (leftovers)
static void close_unused_pipes(int pipes[][2], int num_pipes) {
    for (int i = 0; i < num_pipes; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }
}

// concatenate multiple '<' files into one temp stream, in order
static int build_combined_input(Command *cmd) {
    char tmpl[] = "/tmp/.cshell_in_XXXXXX";
    int tmp_fd = mkstemp(tmpl);
    if (tmp_fd < 0) { perror("cshell"); return -1; }
    unlink(tmpl);   // fd stays open and usable, no leftover file on disk

    for (int k = 0; k < cmd->in_count; k++) {
        int fd = open(cmd->in_files[k], O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "cshell: no such file or directory\n");
            close(tmp_fd);
            return -1;
        }
        char buf[4096];
        ssize_t n;
        while ((n = read(fd, buf, sizeof(buf))) > 0) write(tmp_fd, buf, n);
        close(fd);
    }
    lseek(tmp_fd, 0, SEEK_SET);
    return tmp_fd;
}

// builtins run as a normal function call, not exec'd -- lets them work as
// pipeline stages (e.g. "reveal | peek -n"). runs only inside a fork'd
// child, so it never affects the real shell's own state (cwd, etc).
static bool run_builtin_in_child(Command *cmd) {
    if (strcmp(cmd->name, "hop") == 0)    { execute_hop(cmd);    return true; }
    if (strcmp(cmd->name, "reveal") == 0) { execute_reveal(cmd); return true; }
    if (strcmp(cmd->name, "peek") == 0)   { execute_peek(cmd);   return true; }
    if (strcmp(cmd->name, "locate") == 0) { execute_locate(cmd); return true; }
    return false;
}

// never returns: execs cmd, or prints "not found" (to stderr) and exits
static void run_exec(Command *cmd) {
    char *cmd_name = cmd->argv[0];
    bool force_path = false;

    if (cmd_name[0] == '%') {
        force_path = true;
        cmd_name++;
        cmd->argv[0] = cmd_name;
    }

    if (strchr(cmd_name, '/') != NULL) {
        execv(cmd_name, cmd->argv);              // literal path
    } else if (force_path) {
        execvp(cmd_name, cmd->argv);              // '%name' -> PATH only
    } else {
        char cwd[1030];
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            char cwd_path[2060];
            snprintf(cwd_path, sizeof(cwd_path), "%s/%s", cwd, cmd_name);
            if (access(cwd_path, X_OK) == 0) execv(cwd_path, cmd->argv);   // cwd first
        }
        execvp(cmd_name, cmd->argv);              // then PATH
    }

    // stderr, not stdout -- so this is never swallowed by pipe redirection
    fprintf(stderr, "cshell: command not found (%s)\n", cmd_name);
    exit(NOT_FOUND_EXIT);
}

// undo the shell's own signal customizations in every child, before exec.
// SIG_IGN survives exec() (unlike a real handler), so without this every
// program we run would inherit the shell's SIGTTIN/SIGTTOU ignoring --
// breaking real job-control behavior (e.g. bg jobs never stopping on tty read)
static void reset_job_control_signals(void) {
    signal(SIGINT,  SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
    signal(SIGTTIN, SIG_DFL);
    signal(SIGTTOU, SIG_DFL);
    signal(SIGCHLD, SIG_DFL);
}

// sets up one stage's redirection, then execs it. never returns.
static void child_exec(Command *cmd, int in_fd, int out_fd,
                        int pipes[][2], int num_pipes) {

    if (cmd->in_count == 1) {
        in_fd = open(cmd->in_files[0], O_RDONLY);
        if (in_fd < 0) { fprintf(stderr, "cshell: no such file or directory\n"); exit(1); }
    } else if (cmd->in_count > 1) {
        in_fd = build_combined_input(cmd);
        if (in_fd < 0) exit(1);
    }
    // NOTE: no /dev/null trick anymore. With real process groups + terminal
    // control, a background job that tries to read the real terminal will
    

    
    if (cmd->out_count <= 1) {
        if (cmd->out_count == 1) {
            int flags = O_WRONLY | O_CREAT;
            flags |= cmd->out_append[0] ? O_APPEND : O_TRUNC;
            out_fd = open(cmd->out_files[0], flags, 0644);
            if (out_fd < 0) { fprintf(stderr, "cshell: unable to create file for writing\n"); exit(1); }
        }
        if (in_fd != STDIN_FILENO)   { dup2(in_fd, STDIN_FILENO); close(in_fd); }
        if (out_fd != STDOUT_FILENO) { dup2(out_fd, STDOUT_FILENO); close(out_fd); }
        close_unused_pipes(pipes, num_pipes);
        if (run_builtin_in_child(cmd)) exit(0);
        run_exec(cmd);
    } else {
        // multiple '>' targets -- tee via a grandchild
        int out_fds[MAX_REDIR];
        for (int k = 0; k < cmd->out_count; k++) {
            int flags = O_WRONLY | O_CREAT;
            flags |= cmd->out_append[k] ? O_APPEND : O_TRUNC;
            out_fds[k] = open(cmd->out_files[k], flags, 0644);
            if (out_fds[k] < 0) { fprintf(stderr, "cshell: unable to create file for writing\n"); exit(1); }
        }

        int tee_pipe[2];
        pipe(tee_pipe);
        pid_t tee_pid = fork();

        if (tee_pid == 0) {
            close(tee_pipe[0]);
            if (in_fd != STDIN_FILENO) { dup2(in_fd, STDIN_FILENO); close(in_fd); }
            dup2(tee_pipe[1], STDOUT_FILENO);
            close(tee_pipe[1]);
            for (int k = 0; k < cmd->out_count; k++) close(out_fds[k]);
            close_unused_pipes(pipes, num_pipes);
            if (run_builtin_in_child(cmd)) exit(0);
        run_exec(cmd);
        }

        close(tee_pipe[1]);
        if (in_fd != STDIN_FILENO) close(in_fd);
        char buf[4096]; ssize_t n;
        while ((n = read(tee_pipe[0], buf, sizeof(buf))) > 0) {
            for (int k = 0; k < cmd->out_count; k++) write(out_fds[k], buf, n);
        }
        close(tee_pipe[0]);
        for (int k = 0; k < cmd->out_count; k++) close(out_fds[k]);
        close_unused_pipes(pipes, num_pipes);
        waitpid(tee_pid, NULL, 0);
        exit(0);
    }
}

// forks every stage, wires up pipes, AND puts them all in one process group
static void launch_pipeline(Pipeline *p, pid_t *pids, int pipes[][2]) {
    int num_pipes = p->nstages - 1;
    pid_t pgid = 0;   // becomes stage 0's pid once known

    for (int i = 0; i < num_pipes; i++) {
        if (pipe(pipes[i]) < 0) {
            perror("cshell: pipe failed");
            for (int j = 0; j < i; j++) { close(pipes[j][0]); close(pipes[j][1]); }
            for (int j = 0; j < p->nstages; j++) pids[j] = -1;
            return;
        }
    }

    for (int i = 0; i < p->nstages; i++) {
        Command *cmd = p->stages[i];
        pid_t pid = fork();

        if (pid == 0) {
            pid_t my_pgid = (i == 0) ? getpid() : pgid;
            setpgid(0, my_pgid);         
            reset_job_control_signals();   // let this program behave normally

            int in_fd  = (i > 0) ? pipes[i - 1][0] : STDIN_FILENO;
            int out_fd = (i < num_pipes) ? pipes[i][1] : STDOUT_FILENO;
            child_exec(cmd, in_fd, out_fd, pipes, num_pipes);
            _exit(1);   // unreachable safety net
        }

        if (i == 0) pgid = pid;
        setpgid(pid, pgid);  
        pids[i] = pid;
    }

    close_unused_pipes(pipes, num_pipes);
}

bool execute_pipeline_fg(Pipeline *p) {
    if (p == NULL || p->nstages == 0 || p->stages[0]->argc == 0) return false;

    sigset_t old_mask;
    jobs_block_sigchld(&old_mask);   // don't let bg completions print mid-command

    pid_t pids[MAX_STAGES];
    int pipes[MAX_STAGES][2];
    launch_pipeline(p, pids, pipes);

    pid_t pgid = pids[0];
    int job_number = jobs_add(pids, p, false);   // track it in case it gets Stopped

    terminal_give_to(pgid);   
    bool single_not_found = false;
    bool stopped = false;

    for (int i = 0; i < p->nstages; i++) {
        if (pids[i] < 0) continue;
        int status;
        pid_t w;
        // retry on EINTR -- a signal hitting the SHELL itself (not this
        // child) must not make us read status before waitpid actually set it
        do {
            w = waitpid(pids[i], &status, WUNTRACED);
        } while (w < 0 && errno == EINTR);
        if (w < 0) continue;   // real error (e.g. ECHILD) -- give up on this one

        if (WIFSTOPPED(status)) {
            stopped = true;   // rest of the group got the same signal
            break;
        }
        if (p->nstages == 1 && WIFEXITED(status) && WEXITSTATUS(status) == NOT_FOUND_EXIT) {
            single_not_found = true;   
        }
    }

    terminal_reclaim();   

    if (stopped) {
        jobs_mark_stopped(pgid);
        Job *j = jobs_find_by_pgid(pgid);
        printf("[%d] + Stopped    %s\n", job_number, j ? j->cmdline : "");
        fflush(stdout);
    } else {
        jobs_remove(pgid);   // finished normally -- nothing left to track
    }

    jobs_unblock_sigchld(&old_mask);
    return single_not_found;
}

void execute_pipeline_bg(Pipeline *p) {
    if (p == NULL || p->nstages == 0 || p->stages[0]->argc == 0) return;

    pid_t pids[MAX_STAGES];
    int pipes[MAX_STAGES][2];
    launch_pipeline(p, pids, pipes);   // never given the terminal -> stays background

    int job_number = jobs_add(pids, p, true);
    if (job_number > 0) {
        printf("[%d] %d\n", job_number, (int)pids[0]);   
        fflush(stdout);
    }
}