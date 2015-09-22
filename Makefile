CC = gcc

LOADLIBES = -lm

CFLAGS = -Wall -g

xssh: xssh.o

clean:
	rm -f xssh *.o
