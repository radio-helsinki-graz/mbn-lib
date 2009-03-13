include Makefile.inc


auto:
	@if [ "`uname`" = 'Linux' ]; then\
	  echo "Detected target: linux";\
	  ${MAKE} PLATFORM=linux all;\
	else\
	  echo "Detected target: mingw";\
	  ${MAKE} PLATFORM=mingw all;\
	fi

all: main

main: main.c src/mbn.a
	gcc ${CFLAGS} main.c -Isrc src/mbn.a ${LFLAGS} -o main

src/mbn.a: force_look
	${MAKE} -C src/ PLATFORM=${PLATFORM} static

shared:
	${MAKE} -C src/ PLATFORM=${PLATFORM} shared

clean:
	rm -f main
	${MAKE} -C src/ PLATFORM=${PLATFORM} clean

force_look:

