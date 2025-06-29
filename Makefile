CC = gcc
CFLAGS = -Wall -Wextra -g

all: main

main: main.o jinsock.o
	$(CC) $(CFLAGS) -o js5 main.o jinsock.o

main.o: main.c jinsock.h
jinsock.o: jinsock.c jinsock.h

clean:
	rm -f *.o main
