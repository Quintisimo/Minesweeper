CC = gcc
CFLAGS = -Wall -Werror

make:
	rm -f client server
	$(CC) -o client client.c $(CFLAGS)
	$(CC) -o server server.c -lpthread $(CFLAGS)

clean:
	rm -f client server