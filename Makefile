
all: main

main: main.c src/mbn.a
	gcc main.c -o main -Isrc src/mbn.a -lpthread

src/mbn.a: force_look
	${MAKE} -C src/

clean:
	rm main main.o

force_look:

