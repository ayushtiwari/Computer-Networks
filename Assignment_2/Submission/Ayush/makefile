server: src/server.c
	gcc -o ./product/server ./src/server.c

client: src/client.c
	gcc -o ./product/client ./src/client.c

product: server client
	chmod +x run	

clean:
	rm ./product/server ./product/client