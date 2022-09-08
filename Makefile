FILEMAP = filemap
CC = gcc
CFLAGS = -O2 -Wall -Wextra -g -pedantic-errors

.PHONY: all clean

all: ${FILEMAP}.c
	${CC} ${CFLAGS} ${FILEMAP}.c -o ${FILEMAP}	

clean:
	rm ${FILEMAP}
