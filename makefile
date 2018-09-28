CC = gcc
CFLAGS = -lpthread

make:
	rm -f client server
	$(CC) -o client client.c
	$(CC) -o server server.c $(CFLAGS)

clean:
	rm -f client server