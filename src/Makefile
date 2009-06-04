include ../Makefile.inc

OUTPUT  =
HEADERS = address.h codec.h mbn.h object.h
OBJECTS = address.o codec.o mbn.o object.o
DYNAMIC = libmbn.so


ifdef IF_ETHERNET
	OBJECTS += ${IF_ETHERNET}.o
endif
ifdef IF_TCP
	OBJECTS += ${IF_TCP}.o
endif


ifdef BUILD_STATIC
	OUTPUT = libmbn.a
else
	ifeq (${PLATFORM}, linux)
		OUTPUT = libmbn.so
	else
		OUTPUT = mbn.dll
	endif
endif


build: ${OBJECTS}
ifdef BUILD_STATIC
	ar rcs ${OUTPUT} ${OBJECTS}
else
	${CC} -shared -o ${OUTPUT} ${OBJECTS} ${LFLAGS}
endif


p:
	${MAKE} -C ..


%.o: %.c ${HEADERS} Makefile
	${CC} -c ${CFLAGS} -DMBNP_BUILD $*.c


mbn.h: mbn.h.in ../Makefile.inc
	sed 's/#define MBNP_PLATFORM/#define MBNP_${PLATFORM}/' mbn.h.in > mbn.h
ifndef BUILD_STATIC
	sed 's/#define MBNP_STATIC/#undef MBNP_STATIC/' < mbn.h > mbn.h-
	mv mbn.h- mbn.h
endif
ifdef IF_ETHERNET
	sed 's/#undef MBN_IF_ETHERNET/#define MBN_IF_ETHERNET ${IF_ETHERNET}/' < mbn.h > mbn.h-
	mv mbn.h- mbn.h
endif
ifdef IF_TCP
	sed 's/#undef MBN_IF_TCP/#define MBN_IF_TCP ${IF_TCP}/' < mbn.h > mbn.h-
	mv mbn.h- mbn.h
endif
ifdef MANUFACTURERID
	sed 's/#undef MBN_MANUFACTURERID/#define MBN_MANUFACTURERID ${MANUFACTURERID}/' < mbn.h > mbn.h-
	mv mbn.h- mbn.h
endif


clean:
	rm -f mbn.h ${OBJECTS} ${STATIC} ${OUTPUT}

