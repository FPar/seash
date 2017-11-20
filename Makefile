CC = gcc
CFLAGS = -Wall -pedantic -g
SRCS = seash.c getcommand.c util.c list.c command.c execute.c sig.c
OBJS = $(SRCS:.c=.o)

.PHONY: all
all : seash

seash: $(OBJS)

seash.o: getcommand.h command.h util.h execute.h
getcommand.o: getcommand.h command.h util.h list.h
util.o: util.h
list.o: list.h util.h
command.o: command.h list.h util.h
execute.o: execute.h
sig.o: sig.h

.PHONY: clean
clean :
	rm -f *.o seash
