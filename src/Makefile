include ../Makefile.inc

STATIC  = mbn.a
HEADERS = address.h codec.h mbn.h object.h
OBJECTS = address.o codec.o mbn.o object.o
DYNAMIC = libmbn.so

ifeq (${PLATFORM}, linux)
	OBJECTS += if_ethernet.o if_tcp.o
else
	OBJECTS += if_tcp.o
	DYNAMIC = mbn.dll
endif

static: ${STATIC}

p:
	${MAKE} -C ..

${STATIC}: ${OBJECTS}
	ar rcs ${STATIC} ${OBJECTS}

%.o: %.c ${HEADERS} Makefile
	gcc -c ${CFLAGS} -DMBN_BUILD $*.c

shared: mbn.h
	sed 's/#define MBNP_STATIC/#undef MBNP_STATIC/' < mbn.h > mbn.h-
	mv mbn.h- mbn.h
	${MAKE} ${OBJECTS}
	gcc -shared -o ${DYNAMIC} ${OBJECTS} ${LFLAGS}

mbn.h: mbn.h.in
	sed 's/#define MBNP_PLATFORM/#define MBNP_${PLATFORM}/' mbn.h.in > mbn.h

clean:
	rm -f mbn.h ${OBJECTS} ${STATIC} ${DYNAMIC}

