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
    int numbytes, rv;
    char buff[MAXDATASIZE];

    char hostname[64] = "www.iitkgp.ac.in";

    printf("Enter hostname: ");
    scanf("%s", hostname);

    if(argc>=2) {
        strcpy(hostname, argv[1]);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    if((rv=getaddrinfo("127.0.0.1", PORT, &hints, &servinfo)!=0)) {
        printf("Error : %s\n", gai_strerror(rv));
    }

    for(p=servinfo; p!=NULL; p=p->ai_next) {
        if((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol))==-1) {
            perror("client: socket");
            continue;
        }
        break;
    }

    if(p==NULL) {
        fprintf(stderr, "client: failed to connect\n");
        exit(1);
    }

    // Send the host name
    if((numbytes=sendto(sockfd, hostname, strlen(hostname), 0, p->ai_addr, p->ai_addrlen))==-1) {
        perror("client: sendto");
        exit(1);
    }

    // Receive the ip address
    if((numbytes=recvfrom(sockfd, buff, MAXDATASIZE-1, 0, p->ai_addr, &(p->ai_addrlen)))==-1) {
        perror("recvfrom");
        exit(1);
    }

    printf("hostname: %s, ipaddr: %s\n", hostname, buff);

    freeaddrinfo(servinfo);

    close(sockfd);

    return 0;

}