all:
	gcc server.c -o server -lpthread -lsqlite3
	gcc client.c -o client -lpthread
clean:
	rm -f server client
