CC ?= $(CROSS_COMPILE)gcc
LDFLAGS ?= -pthread
CFLAGS ?= -g -Wall
CFLAGS += -DUSE_AESD_CHAR_DEVICE=1
TARGET ?= aesdsocket
all: clean
	${CC} ${CFLAGS} ${LDFLAGS} -c -o ${TARGET}.o ${TARGET}.c
	${CC} ${CFLAGS} -I/ ${LDFLAGS} -o ${TARGET} ${TARGET}.o

clean:
	rm -rf ${TARGET}.o ${TARGET}

