CFLAGS  = -Wall -Wextra -std=c89 -pedantic
HEADERS = address.h codec.h mbn.h object.h
OBJECTS = address.o codec.o if_ethernet.o mbn.o object.o
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

