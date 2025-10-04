CC = gcc
CFLAGS = -pthread -Wall
OBJ = client server

all: $(OBJ)

client: client.c
	$(CC) $(CFLAGS) -o client client.c

server: server.c
	$(CC) $(CFLAGS) -o server server.c

clean:
	rm -f $(OBJ)
