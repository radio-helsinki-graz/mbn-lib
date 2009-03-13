include ../Makefile.inc

STATIC  = mbn.a
HEADERS = address.h codec.h mbn.h object.h
OBJECTS = address.o codec.o mbn.o object.o


all: ${STATIC}

p:
	${MAKE} -C ..

${STATIC}: ${OBJECTS}
	ar rcs ${STATIC} ${OBJECTS}

%.o: %.c ${HEADERS} Makefile
	gcc -c ${CFLAGS} $*.c

mbn.h: mbn.h.in
	sed 's/#define MBNP_PLATFORM/#define MBNP_${PLATFORM}/' mbn.h.in > mbn.h

clean:
	rm -f mbn.h ${OBJECTS} ${STATIC}

