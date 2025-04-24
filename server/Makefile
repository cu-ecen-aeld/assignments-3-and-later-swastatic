CC ?= $(CROSS_COMPILE)gcc
LDFLAGS ?= -lpthread -lrt
CFLAGS ?= -g -Wall
TARGET ?= aesdsocket
all: clean
	${CC} ${CFLAGS} ${LDFLAGS} -c -o ${TARGET}.o ${TARGET}.c
	${CC} ${CFLAGS} -I/ ${LDFLAGS} -o ${TARGET} ${TARGET}.o

clean:
	rm -rf ${TARGET}.o ${TARGET}

