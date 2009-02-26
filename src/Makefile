CFLAGS  = -Wall -Wextra -std=c89 -pedantic
HEADERS = mbn.h
OBJECTS = mbn.o codec.o ethernet.o
STATIC  = mbn.a

all: ${STATIC}

p:
	${MAKE} -C ..

${STATIC}: ${OBJECTS}
	ar rcs ${STATIC} ${OBJECTS}

%.o: %.c ${HEADERS} Makefile
	gcc -c ${CFLAGS} $*.c

clean:
	rm -f ${OBJECTS} ${STATIC}

