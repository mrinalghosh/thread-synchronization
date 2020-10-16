CC=gcc -Werror -Wall -g 
all: threadlib main
	$(CC) -o main main.o threads.o

main: main.c
	$(CC) -c -o main.o main.c

threadlib: threads.c
	$(CC) -c -o threads.o threads.c

clean:
	rm threads.o main.o main
