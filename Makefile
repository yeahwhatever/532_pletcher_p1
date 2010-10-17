CC=g++

CFLAGS=-Wall -W -g -Werror 

LOADLIBES=-lsocket -lnsl

all: client server

client: client.c raw.c
	$(CC) client.c raw.c $(LOADLIBES) $(CFLAGS) -o client

server: server.c 
	$(CC) server.c $(LOADLIBES) $(CFLAGS) -o server

clean:
	rm -f client server *.o

