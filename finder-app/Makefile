CC=$(CROSS_COMPILE)gcc
all: clean
	${CC} -g -Wall -c -o writer.o writer.c
	${CC} -g -Wall -I/ -o writer writer.o

clean:
	rm -rf writer.o writer

