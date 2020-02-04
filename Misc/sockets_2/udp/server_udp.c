#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#define PORT "3490"
#define BACKLOG 10
#define MAXDATASIZE 100

void *get_in_addr(struct sockaddr *sa) {
    if(sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(void) {

    int sockfd, numbytes;
    struct addrinfo hints, *servinfo, *p;
    int yes = 1;
    char s[INET6_ADDRSTRLEN], buff[MAXDATASIZE];
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_flags = AI_PASSIVE;
    hints.ai_socktype = SOCK_DGRAM;

    if((rv=getaddrinfo(NULL, PORT, &hints, &servinfo)!=0)) {
        printf("Error : %s\n", gai_strerror(rv));
    }

    for(p=servinfo; p!=NULL; p=p->ai_next) {
        if((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol))==-1) {
            perror("server: socket");
            continue;
        }

        if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))==-1) {
            perror("setsockopt");
            exit(1);
        }

        if(bind(sockfd, p->ai_addr, p->ai_addrlen)==-1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo);

    if(p==NULL) {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    printf("server waiting for connections...\n");

    while(1) {
        struct sockaddr_storage their_addr;
        socklen_t addr_len;

        addr_len = sizeof their_addr;

        if((numbytes=recvfrom(sockfd, buff, MAXDATASIZE-1, 0, (struct sockaddr*)&their_addr, &addr_len))==-1) {
            perror("recvfrom");
            exit(1);
        }

        buff[numbytes] = '\0';

        if(!fork()) {
            // Get ip address
            struct hostent *h;

            if ((h=gethostbyname(buff)) == NULL) {  // get the host info
                herror("gethostbyname");
                exit(1);
            }

            char *ipstr = inet_ntoa(*((struct in_addr *)h->h_addr));

            // Send ip back
            if((numbytes=sendto(sockfd, ipstr, strlen(ipstr), 0, (struct sockaddr*)&their_addr, their_addr.ss_len))==-1) {
                perror("client: sendto");
                exit(1);
            }

            close(sockfd);
        }
        
    }

    

    return 0;
}