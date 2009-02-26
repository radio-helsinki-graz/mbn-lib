OBJECTS = mbn.o ethernet.o
STATIC  = mbn.a

all: ${STATIC}

p:
	${MAKE} -C ..

${STATIC}: ${OBJECTS}
	ar rcs ${STATIC} ${OBJECTS}

%.o: %.c
	gcc -c $*.c

clean:
	rm ${OBJECTS} ${STATIC}

