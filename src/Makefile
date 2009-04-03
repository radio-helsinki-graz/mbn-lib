include ../Makefile.inc

OUTPUT  =
HEADERS = address.h codec.h mbn.h object.h
OBJECTS = address.o codec.o mbn.o object.o
DYNAMIC = libmbn.so


ifeq (${PLATFORM}, linux)
	OBJECTS += if_eth_linux.o if_tcp.o if_scan.o
else
	OBJECTS += if_eth_wpcap.o if_tcp.o
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


clean:
	rm -f mbn.h ${OBJECTS} ${STATIC} ${OUTPUT}

