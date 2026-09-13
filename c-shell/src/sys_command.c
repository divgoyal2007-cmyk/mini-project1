#include "sys_command.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "jobs.h"

#define NOT_FOUND_EXIT 127
static void close_unused_pipes(int pipes[][2], int num_pipes) {
    for (int i = 0; i < num_pipes; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }
}


static int build_combined_input(Command *cmd) {
    char tmpl[] = "/tmp/.cshell_in_XXXXXX";
    int tmp_fd = mkstemp(tmpl);
    if (tmp_fd < 0) {
        perror("cshell");
        return -1;
    }
    unlink(tmpl); // fd stays valid, no leftover file on disk

    for (int k = 0; k < cmd->in_count; k++) {
        int fd = open(cmd->in_files[k], O_RDONLY);
        if (fd < 0) {
            printf("cshell: no such file or directory\n");
            close(tmp_fd);
            return -1;
        }
        char buf[4096];
        ssize_t n;
        while ((n = read(fd, buf, sizeof(buf))) > 0) {
            write(tmp_fd, buf, n);
        }
        close(fd);
    }
    lseek(tmp_fd, 0, SEEK_SET);
    return tmp_fd;
}

// Never returns: execs cmd, or prints the "not found" error and exits.
static void run_exec(Command *cmd) {
    char *cmd_name = cmd->argv[0];
    bool force_path = false;

    if (cmd_name[0] == '%') {
        force_path = true;
        cmd_name++;
        cmd->argv[0] = cmd_name;
    }

    if (strchr(cmd_name, '/') != NULL) {
        execv(cmd_name, cmd->argv);
    } else if (force_path) {
        execvp(cmd_name, cmd->argv);
    } else {
        char cwd[1030];
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            char cwd_path[2060];
            snprintf(cwd_path, sizeof(cwd_path), "%s/%s", cwd, cmd_name);
            if (access(cwd_path, X_OK) == 0) {
                execv(cwd_path, cmd->argv);
            }
        }
        execvp(cmd_name, cmd->argv);
    }

    fprintf(stderr, "cshell: command not found (%s)\n", cmd_name);
    exit(NOT_FOUND_EXIT);
}
static void reset_job_control_signals(void){
    signal(SIGINT,SIG_DFL);
    signal(SIGQUIT,SIG_DFL);
    signal(SIGTSTP,SIG_DFL);
    signal(SIGTTIN,SIG_DFL);
    signal(SIGTTOU,SIG_DFL);
    signal(SIGCHLD,SIG_DFL);
}
static void child_exec(Command *cmd,int in_fd,int out_fd,int pipes[][2],int num_pipes,bool background){
if(cmd->in_count==1){
    in_fd=open(cmd->in_files[0],O_RDONLY);
    if(in_fd < 0){
        fprintf(stderr, "cshell: no such file or directory\n");
        exit(1);
    }
}
else if(cmd->in_count > 1){
    in_fd=build_combined_input(cmd);
    if(in_fd < 0)exit(1);
}
if(background && in_fd == STDIN_FILENO){
    int devnull=open("/dev/null",O_RDONLY);
    if(devnull >=0)in_fd=devnull;
}
 if (cmd->out_count <= 1) {
        if (cmd->out_count == 1) {
            int flags = O_WRONLY | O_CREAT;
            flags |= cmd->out_append[0] ? O_APPEND : O_TRUNC;
            out_fd = open(cmd->out_files[0], flags, 0644);
            if (out_fd < 0) {
                printf("cshell: unable to create file for writing\n");
                exit(1);
            }
        }
 
        if (in_fd != STDIN_FILENO)   { dup2(in_fd, STDIN_FILENO); close(in_fd); }
        if (out_fd != STDOUT_FILENO) { dup2(out_fd, STDOUT_FILENO); close(out_fd); }
        close_unused_pipes(pipes, num_pipes);
 
        run_exec(cmd);
    }
else{
    int out_fds[MAX_REDIR];
    for(int k=0;k<cmd->out_count;k++){
        int flags=O_WRONLY | O_CREAT;
        flags |= cmd->out_append[k] ? O_APPEND : O_TRUNC;
        out_fds[k]=open(cmd->out_files[k],flags, 0644);
        if (out_fds[k] < 0) {
                printf("cshell: unable to create file for writing\n");
                exit(1);
            }
    }
    int tee_pipe[2];
    pipe(tee_pipe);
    pid_t tee_pid=fork();
    if (tee_pid == 0) {
            close(tee_pipe[0]);
            if (in_fd != STDIN_FILENO) { dup2(in_fd, STDIN_FILENO); close(in_fd); }
            dup2(tee_pipe[1], STDOUT_FILENO);
            close(tee_pipe[1]);
            for (int k = 0; k < cmd->out_count; k++) close(out_fds[k]);
            close_unused_pipes(pipes, num_pipes);
            run_exec(cmd);
        }
    close(tee_pipe[1]);
     if (in_fd != STDIN_FILENO) close(in_fd);
        char buf[4096];
        ssize_t n;
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


// Forks every stage of the pipeline, wiring pipes between them.
// Fills pids[0..p->nstages) with the child pids (or -1 on pipe failure).
static void launch_pipeline(Pipeline *p, bool background, pid_t *pids, int pipes[][2]) {
    int num_pipes = p->nstages - 1;
 
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
            reset_job_control_signals();
            int in_fd  = (i > 0) ? pipes[i - 1][0] : STDIN_FILENO;
            int out_fd = (i < num_pipes) ? pipes[i][1] : STDOUT_FILENO;
            child_exec(cmd, in_fd, out_fd, pipes, num_pipes, background);
            _exit(1); // unreachable: child_exec never returns
        }
        pids[i] = pid;
    }
 
    close_unused_pipes(pipes, num_pipes);
}
 
bool execute_pipeline_fg(Pipeline *p) {
    if (p == NULL || p->nstages == 0 || p->stages[0]->argc == 0) {
        return false;
    }
 
    // Block SIGCHLD so no background-job completion prints in the middle
    // of this foreground command's output; we reap our own pids directly
    // below, then unblock, which flushes any pending background reports.
    sigset_t old_mask;
    jobs_block_sigchld(&old_mask);
 
    pid_t pids[MAX_STAGES];
    int pipes[MAX_STAGES][2];
    launch_pipeline(p, false, pids, pipes);
 
    bool single_not_found = false;
    for (int i = 0; i < p->nstages; i++) {
        if (pids[i] < 0) continue;
        int status;
        waitpid(pids[i], &status, 0);
        if (p->nstages == 1 && WIFEXITED(status) && WEXITSTATUS(status) == NOT_FOUND_EXIT) {
            single_not_found = true;
        }
    }
 
    jobs_unblock_sigchld(&old_mask);
    return single_not_found;
}
 
void execute_pipeline_bg(Pipeline *p) {
    if (p == NULL || p->nstages == 0 || p->stages[0]->argc == 0) {
        return;
    }
 
    pid_t pids[MAX_STAGES];
    int pipes[MAX_STAGES][2];
    launch_pipeline(p, true, pids, pipes);
 
    int job_number = jobs_add(pids, p, p->stages[0]->name);
    if (job_number > 0) {
        printf("[%d] %d\n", job_number, (int)pids[0]);
        fflush(stdout);
    }
}
 
  
