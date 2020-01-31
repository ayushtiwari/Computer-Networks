#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define PORT "3490"

#define MAXDATASIZE 100

void *get_in_addr(struct sockaddr *sa) {
    if(sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[]) {
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    char s[INET6_ADDRSTRLEN];
    int numbytes, rv;
    char buff[MAXDATASIZE];

    if(argc!=2) {
        fprintf(stderr, "usage: client hostname\n");
        exit(1);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if((rv=getaddrinfo(argv[1], PORT, &hints, &servinfo)!=0)) {
        printf("Error : %s\n", gai_strerror(rv));
    }

    for(p=servinfo; p!=NULL; p=p->ai_next) {
        if((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol))==-1) {
            perror("client: socket");
            continue;
        }

        if(connect(sockfd, p->ai_addr, p->ai_addrlen)==-1) {
            close(sockfd);
            perror("client: connect");
            continue;
        }

        break;
    }

    if(p==NULL) {
        fprintf(stderr, "client: failed to connect\n");
        exit(1);
    }

    inet_ntop(p->ai_family, get_in_addr(p->ai_addr), s, sizeof(s));

    printf("client connecting to %s\n", s);

    freeaddrinfo(servinfo);

    if((numbytes=recv(sockfd, buff, MAXDATASIZE-1, 0))==-1) {
        perror("client: recv");
        exit(1);
    }

    buff[numbytes] = '\0';

    printf("client recieved %s\n", buff);

    close(sockfd);

    return 0;

}