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
#include <dirent.h> 
#include <sys/select.h>

#define PORT "3490"

void *get_in_addr(struct sockaddr *sa) {
    if(sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

/**
 * Initializes and binds the server
 * @return: Socket file descriptor, -2 for incorrecto call, -3 for binding error
 * @param(type): Type of connection ["UDP", "TCP"]
 */
int init_server(char *type) {
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int yes = 1;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_flags = AI_PASSIVE;

    if(strcmp(type, "TCP")==0) {
        hints.ai_socktype = SOCK_STREAM;
    } else if(strcmp(type, "UDP")==0) {
        hints.ai_socktype = SOCK_DGRAM;
    } else {
	fprintf(stderr, "incorrect call to init_server");
        return -2;
    }

    if((rv=getaddrinfo(NULL, PORT, &hints, &servinfo)!=0))
        printf("Error : %s\n", gai_strerror(rv));

    for(p=servinfo; p!=NULL; p=p->ai_next) {
        if((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol))==-1) {
            perror("server: socket");
            continue;
        }

        if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))==-1) {
            perror("server: setsockopt");
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
        fprintf(stderr, "%s server: failed to bind\n", type);
        return -3;
    }

    return sockfd;
}
/**
 * Function representing the image sending protocol
 * @return: 0 on success
 * @param(new_fd): File descriptor corresponding to the connection
 * @param(their_addr): Client address info
 * @param(their_addr): sizeof client address info 
 */
int tcp_job(int new_fd, struct sockaddr_storage their_addr, socklen_t sin_size) {

    int numbytes;
    char ack[32];
    char sub_dirname[64];

    // Get the sudirectory name
    if((numbytes = recv(new_fd, sub_dirname, sizeof(sub_dirname), 0))==-1) {
        perror("server, subdirectory name recv : ");
        exit(0);
    } 
    sub_dirname[numbytes]='\0';

    // Compute full path
    char full_pathname[64] = "image/";
    strcat(full_pathname, sub_dirname);
    strcat(full_pathname, "/");

    // Open the subdirectory
    struct dirent *de;
    DIR *dr = opendir(full_pathname); 
    if (dr == NULL) { 
        fprintf(stderr, "Could not open current directory\n" ); 
        exit(1);
    }

    printf("\n");

    while ((de = readdir(dr)) != NULL) {
        if(strcmp(de->d_name, ".")!=0 && strcmp(de->d_name, "..")!=0) {

            // Calculate file pathname
            char file_pathname[64];
            strcpy(file_pathname, full_pathname);
            strcat(file_pathname, de->d_name);

            // Send file pathname
            if((numbytes=send(new_fd, file_pathname, strlen(file_pathname), 0))==-1) {
                perror("server, sending file pathname : ");
                return -1;
            }

            // ACK
            if((numbytes=recv(new_fd, ack, sizeof(ack), 0))==-1) {
                perror("server, recv acknowledgement");
                return -1;
            }
            // Open file
            FILE* picture = fopen(file_pathname, "r");
            if(!picture) {
                fprintf(stderr, "Could not open file %s\n", file_pathname);
                return -1;
            }
            
            // Calculate file size
            int size = 0;

            fseek(picture, 0, SEEK_END);
            size = ftell(picture);
            fseek(picture, 0, SEEK_SET);

            // Send file size   
            if((numbytes=send(new_fd, &size, sizeof(int), 0))==-1) {
                perror("server, sending file size : ");
                return -1;
            }

            // ACK
            if((numbytes=recv(new_fd, ack, sizeof(ack), 0))==-1) {
                perror("server, recv acknowledgement");
                return -1;
            }
            
            // // Declare sending buffer
            char send_buffer[size];

            // Send file
            fread(send_buffer, 1, sizeof(send_buffer), picture);

            numbytes = send(new_fd, send_buffer, sizeof(send_buffer), 0);
            // printf("%d bytes sent\n\n", numbytes);

            //ACK
            if((numbytes=recv(new_fd, ack, sizeof(ack), 0))==-1) {
                perror("server, recv acknowledgement");
                return -1;
            }

            fclose(picture);

        }
    }
    
    // Ending transfer	
    char *buff="END";
    if((numbytes=send(new_fd, buff, strlen(buff), 0))==-1) {
        perror("server, sending END : ");
        exit(1);
    }

    closedir(dr);     
    close(new_fd);

    return 0;
}
/**
 * UDP server's job
 * @return: 0 on success
 * @param(sockfd_udp): corresponding socket
 * @param(their_addr): client address info
 * @param(addr_len): client address info size
 * @param(query_hostname): query made by client
 */
int udp_job(int sockfd_udp, struct sockaddr_storage their_addr, socklen_t addr_len, char *query_hostname) {
    int numbytes;
    struct hostent *h;

    if ((h=gethostbyname(query_hostname)) == NULL) {
        herror("udp server: gethostbyname");
        return -1;
    }

    char *ipstr = inet_ntoa(*((struct in_addr *)h->h_addr));
    if((numbytes=sendto(sockfd_udp, ipstr, strlen(ipstr), 0, (struct sockaddr*)&their_addr, sizeof their_addr))==-1) {
        perror("udp server: sendto");
        return -1;
    }

    return 0;
}

int main() {

    int sockfd_udp, sockfd_tcp, numbytes_udp, numbytes_tcp;
    sockfd_tcp = init_server("TCP");
    sockfd_udp = init_server("UDP");

    if(sockfd_udp == -1 || sockfd_udp == -2) {
        perror("init server udp");
        exit(1);
    }

    if(sockfd_tcp == -1 || sockfd_tcp == -2) {
        perror("init server tcp");
        exit(1);
    }

    // printf("%d, %d\n", sockfd_udp, sockfd_tcp);

    if(listen(sockfd_tcp, 10)==-1) {
        perror("listen");
        exit(1);
    }

    int fdmax = sockfd_udp;
    if(fdmax < sockfd_tcp) fdmax = sockfd_tcp;

    fd_set readfds;

    while(1) {

        FD_ZERO(&readfds);

        FD_SET(sockfd_tcp, &readfds);
        FD_SET(sockfd_udp, &readfds);

        if(select(fdmax+1, &readfds, NULL, NULL, NULL)==-1) {
            perror("select");
            exit(1);
        }

        // Serve tcp request
        if(FD_ISSET(sockfd_tcp, &readfds)) {
            struct sockaddr_storage tcp_client_addr;
            socklen_t tcp_client_addr_size = sizeof tcp_client_addr_size;
            int new_fd = accept(sockfd_tcp, (struct sockaddr*)&tcp_client_addr, &tcp_client_addr_size);
            if(!fork()) {
                char s[INET6_ADDRSTRLEN];
                inet_ntop(tcp_client_addr.ss_family, get_in_addr((struct sockaddr*)&tcp_client_addr), s, sizeof(s));
                printf("Sending images to \"%s\"...", s);
                close(sockfd_tcp);
                tcp_job(new_fd, tcp_client_addr, tcp_client_addr_size);
		// printf("Done");
                close(new_fd);
                exit(0);
            } else {
                close(new_fd);
            }
        }

        // Serve UDP request
        if(FD_ISSET(sockfd_udp, &readfds)) {
            char s[INET6_ADDRSTRLEN];
            // UDP JOB
            struct sockaddr_storage their_addr;
            socklen_t addr_len;
            char buff[129];
            int numbytes;

            addr_len = sizeof their_addr;

            if((numbytes=recvfrom(sockfd_udp, buff, 127, 0, (struct sockaddr*)&their_addr, &addr_len))==-1) {
                perror("recvfrom");
                exit(1);
            }
            inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr*)&their_addr), s, sizeof(s));
            printf("Sending ip address to \"%s\"\n", s);

            buff[numbytes] = '\0';
            udp_job(sockfd_udp, their_addr, addr_len, buff);
	    // printf("Done");
        }
    }
    
    close(sockfd_tcp);
    close(sockfd_udp);

}
