download: download.c
	g++ -o download download.c
all: client server download
client: client.c
	gcc -o client client.c
server: server.c
	gcc -o server server.c
clean:
	rm client server download
