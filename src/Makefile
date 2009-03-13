include ../Makefile.inc

STATIC  = mbn.a
HEADERS = address.h codec.h mbn.h object.h
OBJECTS = address.o codec.o mbn.o object.o
DYNAMIC = libmbn.so

ifeq (${PLATFORM}, linux)
	OBJECTS += if_ethernet.o if_tcp.o
else
	DYNAMIC = mbn.dll
endif

static: ${STATIC}

p:
	${MAKE} -C ..

${STATIC}: ${OBJECTS}
	ar rcs ${STATIC} ${OBJECTS}

%.o: %.c ${HEADERS} Makefile
	gcc -c ${CFLAGS} $*.c

shared:
	gcc -shared -o ${DYNAMIC} ${LFLAGS}

mbn.h: mbn.h.in
	sed 's/#define MBNP_PLATFORM/#define MBNP_${PLATFORM}/' mbn.h.in > mbn.h

clean:
	rm -f mbn.h ${OBJECTS} ${STATIC}

