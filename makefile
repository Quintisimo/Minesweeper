CC = gcc
CFLAGS = -lpthread

make: server client
	$(CC) -o client client.c
	$(CC) -o server server.c $(CFLAGS)

clean:
	rm -f client server