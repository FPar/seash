#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <pwd.h>
#include "command.h"
#include "getcommand.h"
#include "util.h"
#include "execute.h"
#include "sig.h"

#define PROMPT "->"
#define DEBUG 0

static int iscd(commandlist *);

int main(int argc, char **argv) {
    setup_signals();

    commandlist *clist;

    while (1) {
        printf("%s ", PROMPT);
        fflush(stdout);
        clist = getcommandlist(stdin);
        if (clist == NULL) {
            if (feof(stdin)) {
                break;
            }
        } else {
#if DEBUG
            print_commandlist(clist);
#endif
            if (valid_commandlist(clist)) {
                if (iscd(clist))
                {
                    char *dir;
                    if(clist->head->args->len) {
                        dir = clist->head->args->head->str;
                    } else {
                        dir = getpwuid(getuid())->pw_dir;
                    }
                    if(chdir(dir)) {
                        perror("cd");
                    }
                } else {
                    execute_commands(clist);
                }
            }
            delete_commandlist(clist);
            free(clist);
        }
    }
    return 0;
}

int iscd(commandlist *clist) {
    return clist != NULL && clist->head != NULL && clist->head->cmd != NULL
        && (strcmp(clist->head->cmd, "cd") == 0);
}
