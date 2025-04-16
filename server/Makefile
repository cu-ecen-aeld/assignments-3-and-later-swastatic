CC=gcc
all: clean
	${CC} -g -Wall -pthread -c -o aesdsocket.o aesdsocket.c
	${CC} -g -Wall -I/ -pthread -o aesdsocket aesdsocket.o

clean:
	rm -rf aesdsocket.o aesdsocket

