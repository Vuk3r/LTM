CC = gcc
CFLAGS = -Wall -Wextra -std=c11
GTK_CFLAGS = $(shell pkg-config --cflags gtk+-3.0)
GTK_LIBS = $(shell pkg-config --libs gtk+-3.0)
CLIENT_LIBS = -lpthread $(GTK_LIBS)
SERVER_LIBS = -lpthread

all: client server

client: client.c
	$(CC) $(CFLAGS) $(GTK_CFLAGS) -o client client.c $(CLIENT_LIBS)

server: server.c
	$(CC) $(CFLAGS) -o server server.c $(SERVER_LIBS)

clean:
	rm -f client server

.PHONY: all clean

