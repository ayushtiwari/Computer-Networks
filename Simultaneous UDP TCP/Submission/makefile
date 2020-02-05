client_tcp: client_tcp.c 
	gcc -o client_tcp client_tcp.c 

client_udp: client_udp.c 
	gcc -o client_udp client_udp.c 

server: server.c 
	gcc -o server server.c 

all: server client_udp client_tcp

clean:
	rm client_tcp
	rm client_udp
	rm -r client_images