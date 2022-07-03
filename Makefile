all: server/server client/client

server/server: server/server.c
	gcc server/server.c -o server/server

client/client: client/client.c
	gcc client/client.c -o client/client

