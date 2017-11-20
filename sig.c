#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include "sig.h"
#include "execute.h"

extern void disable_sigint() {
    sigset_t sig;
    sigemptyset(&sig);
    sigaddset(&sig, SIGINT);
    if(sigprocmask(SIG_BLOCK, &sig, NULL)) {
        perror("disable_sigint");
        exit(-1);
    }
}

extern void enable_sigint() {
    sigset_t sig;
    sigemptyset(&sig);
    sigaddset(&sig, SIGINT);
    if(sigprocmask(SIG_UNBLOCK, &sig, NULL)) {
        perror("enable_sigint");
        exit(-1);
    }
}


static void handle_sig(int sig, siginfo_t *info, void * context) {
    execute_interrupt();
}

extern void setup_signals() {
    disable_sigint();

    struct sigaction sig_action;
    sig_action.sa_sigaction = handle_sig;
    sigemptyset(&sig_action.sa_mask);
    sig_action.sa_flags = SA_SIGINFO;

    if(sigaction(SIGINT, &sig_action, NULL)) {
        perror("setup_signals");
        exit(-1);
    }
}