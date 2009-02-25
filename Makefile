
all:
	gcc -c mbn.c
	gcc main.c -o main mbn.o -lpthread
	./main

