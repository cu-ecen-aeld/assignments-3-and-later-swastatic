CROSS_COMPILE=aarch64-none-linux-gnu-
all: clean
	${CROSS_COMPILE}gcc -g -Wall -c -o writer.o writer.c
	${CROSS_COMPILE}gcc -g -Wall -I/ -o writer writer.o

clean:
	rm -rf writer.o writer

