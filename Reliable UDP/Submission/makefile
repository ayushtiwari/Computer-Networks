library: rsocket.c
	gcc -o rsocket.o -c rsocket.c
	ar rcs librsocket.a rsocket.o


all: user1.c user2.c rsocket.h library
	gcc -o user1 user1.c -L. -lrsocket
	gcc -o user2 user2.c -L. -lrsocket