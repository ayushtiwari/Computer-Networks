server: server_tcp.c
	gcc -o server server_tcp.c

client: client_tcp.c
	gcc -o client client_tcp.c 

final: server client

clean:
	rm -r client_images
	rm server
	rm client