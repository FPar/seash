#ifndef EXECUTE
#define EXECUTE

#include "command.h"

extern void execute_init();
extern void execute_commands(commandlist *list);
extern void execute_interrupt();

#endif