#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <wait.h>
#include <fcntl.h>
#include <errno.h>

#include "list.h"
#include "execute.h"
#include "sig.h"

#define EOE(c, s) if(c) {\
    perror(s);\
    exit(-1);\
}

// This macro is actually not needed in the scope of this program.
// Using this would be useful if more signals would be handled and are not blocked globally.
#define IGNEINTR(c) {\
    int error;\
    do {\
        error = c;\
    } while(error && errno == EINTR);\
}

#define CLOSE(fd) IGNEINTR(close(fd))

typedef struct cur {
    command *com;
    int fds[4];
    int *inpipe;
    int *outpipe;
    int begin;
    int end;
} cursor;

static int execute_fork(cursor *cursor);
static int execute_command(cursor *cursor);
static void execute_wait();
static void cursor_init(cursor *cursor);
static void switch_pipes(cursor *cursor);
static int redirect_stdout(char *file);
static int redirect_stdin(char *file);
static int count_commands(commandlist *list);

struct {
    pid_t *pids;
    int pidindex;
    int pidcount;
} execution;

void execute_commands(commandlist *list) {
    execution.pidcount = 0;
    execution.pidindex = 0;
    int pidlen = count_commands(list);
    pid_t pids[pidlen];
    execution.pids = pids;

    cursor cursor;
    cursor_init(&cursor);

    command *current = list->head;
    while(current) {
        if(!current->next_one) {
            cursor.end = 1;
        }

        cursor.com = current;
        int pid;
        if((pid = execute_fork(&cursor)) <= 0) {
            break;
        } else {
            pids[execution.pidcount] = pid;
            ++execution.pidcount;
        }

        current = current->next_one;
        cursor.begin = 0;

        // inpipe was fully closed in this process and oldpipe is new inpipe.
        switch_pipes(&cursor);
    }

    execute_wait();
}

void execute_interrupt() {
    for(int i = execution.pidindex; i < execution.pidcount; ++i) {
        kill(execution.pids[i], SIGINT);
    }
}

int execute_fork(cursor *cursor) {
    int fds[2];
    EOE(pipe(fds), "pipe");

    int *inpipe = cursor->inpipe, *outpipe = cursor->outpipe;
    if(!cursor->end) {
        EOE(pipe(outpipe), "pipe");
    }

    pid_t child_pid = fork();
    if(child_pid) {
        // Close input end of self pipe.
        CLOSE(fds[1]);

        // Close reading end of outpipe, if there is one, duplicated in child.
        // Writing end is already closed by previous iteration.
        if(!cursor->end) {
            CLOSE(outpipe[1]);
        }

        // Close writing end of input pipe, if there is one, also duplicated in child.
        if(!cursor->begin) {
            CLOSE(inpipe[0]);
        }
        
        // Try to read from self pipe.
        char buf;
        int count;
        do {
            count = read(fds[0], &buf, sizeof(buf));
            // If something is read, exec (or something else) failed.
            if(count > 0) {
                CLOSE(fds[0]);
                return -1;
            } else if (count < 0 && errno != EINTR) {
                // Something bad happened.
                perror("exec");
                exit(-1);
            }
        } while(count < 0 && errno == EINTR);

        CLOSE(fds[0]);
    } else {
        // We don't read from self pipe.
        CLOSE(fds[0]);
        // Close the writing self pipe if exec succeeds, so parent sees it.
        fcntl(fds[1], F_SETFD, FD_CLOEXEC);

        if(execute_command(cursor)) {
            IGNEINTR(write(fds[1], "", 1));
            exit(1);
        }
    }

    return child_pid;
}

int execute_command(cursor *cursor) {
    command *com = cursor->com;

    int *inpipe = cursor->inpipe, *outpipe = cursor->outpipe;

    // Replaced stdin/stdout with pipes if necessary.
    if(!cursor->begin) {
        if(dup2(inpipe[0], STDIN_FILENO) < 0) {
            perror("execute_command");
            return -1;
        }
        CLOSE(inpipe[0]);
        CLOSE(inpipe[1]);
    }

    if(!cursor->end) {
        if(dup2(outpipe[1], STDOUT_FILENO) < 0) {
            perror("execute_command");
            return -1;
        }
        CLOSE(outpipe[0]);
        CLOSE(outpipe[1]);
    }

    // Build argv.
    char *args[com->args->len + 2];
    command_args_to_vector(com, args);

    // Redirect stdin/stdout if necessary.
    if(com->in) {
        if(redirect_stdin(com->in))
            return -1;
    }

    if(com->out) {
        if(redirect_stdout(com->out))
            return -1;
    }

    // Reenable SIGINT.
    enable_sigint();

    execvp(com->cmd, args);
    if(errno == ENOENT) {
        // File not found.
        fprintf(stderr, "No such file \"%s\".\n", com->cmd);
    } else {
        // Something else.
        perror(cursor->com->cmd);
    }
    // Either way it's an error that we are still here.
    return -1;
}

void execute_wait() {
    // SIGINT should only work here.
    enable_sigint();
    for(; execution.pidindex < execution.pidcount; ++execution.pidindex) {
        int error;
        do {
            error = waitpid(execution.pids[execution.pidindex], NULL, 0);
        } while(error && errno == EINTR);
    }
    disable_sigint();
}

void cursor_init(cursor *cursor) {
    cursor->begin = 1;
    cursor->end = 0;
    cursor->inpipe = cursor->fds;
    cursor->outpipe = cursor->fds + 2;
}

void switch_pipes(cursor *cursor) {
    int *tmp = cursor->inpipe;
    cursor->inpipe = cursor->outpipe;
    cursor->outpipe = tmp;
}

int redirect_stdout(char *file) {
    int fd = open(file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if(fd == -1) {
        perror("redirect_stdout");
        return -1;
    }

    if(dup2(fd, STDOUT_FILENO) < 0) {
        perror("redirect_stdout");
        return -1;
    }
    CLOSE(fd);

    return 0;
}

int redirect_stdin(char *file) {
    int fd = open(file, O_RDONLY);
    if(fd == -1) {
        perror("redirect_stdin");
        return -1;
    }

    if(dup2(fd, STDIN_FILENO) < 0) {
        perror("redirect_stdin");
        return -1;
    }
    CLOSE(fd);

    return 0;
}

int count_commands(commandlist *list) {
    int i = 0;
    command *current = list->head;
    while(current) {
        ++i;
        current = current->next_one;
    }
    return i;
}