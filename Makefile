CC = gcc
CFLAGS = -Wall

SRC = ./src
BUILD = ./build
BIN = ${BUILD}/bin
OBJ = ${BUILD}/obj

SRCS = $(shell find ${SRC} -name *.c)
OBJS = $(patsubst ${SRC}/%.c,${OBJ}/%.o,${SRCS})


all: mksubdir main

mksubdir:
	mkdir -p ${BIN}
	mkdir -p `find ${SRC} -type d | sed 's/$(subst /,\/,${SRC})/$(subst /,\/,${OBJ})/'`

main: ${OBJS}
	${CC} -lwebsockets $^ -o ${BIN}/$@

${OBJ}/%.o: ${SRC}/%.c
	${CC} ${CFLAGS} -c $< -o $@

run:
	${BIN}/main

clean:
	rm -fr ${BUILD}/
