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

#define PORT "3490"

void *get_in_addr(struct sockaddr *sa) {
    if(sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

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


    // printf("full_pathname: %s\n", full_pathname);
    // Open the subdirectory
    struct dirent *de;
    DIR *dr = opendir(full_pathname); 
    if (dr == NULL) { 
        fprintf(stderr, "Could not open current directory\n" ); 
        exit(1);
    } 

    // Check no. of files in directory
    int no_of_files = 0;
    while ((de = readdir(dr)) != NULL) {
        if(strcmp(de->d_name, ".")!=0 && strcmp(de->d_name, "..")!=0) {
            no_of_files++;
        }
    }

    // Send the no. of files
    if((numbytes = send(new_fd, &no_of_files, sizeof(int), 0))==-1) {
        perror("server, no_of_files send : ");
        exit(1);
    }

    rewinddir(dr);
    printf("\n");

    while ((de = readdir(dr)) != NULL) {
        if(strcmp(de->d_name, ".")!=0 && strcmp(de->d_name, "..")!=0) {

            // Calculate file pathname
            char file_pathname[64];
            strcpy(file_pathname, full_pathname);
            strcat(file_pathname, de->d_name);

            printf("Sending %s...\n", file_pathname);
            
            // Send file pathname
            if((numbytes=send(new_fd, file_pathname, strlen(file_pathname), 0))==-1) {
                perror("server, sending file pathname : ");
                exit(1);
            }

            // Recv acknowledgement
            if((numbytes=recv(new_fd, ack, sizeof(ack), 0))==-1) {
                perror("server, recv acknowledgement");
                exit(1);
            }
            // Open file
            FILE* picture = fopen(file_pathname, "r");
            if(!picture) {
                fprintf(stderr, "Could not open file %s\n", file_pathname);
                exit(1);
            }
            
            // Calculate file size
            int size = 0;

            fseek(picture, 0, SEEK_END);
            size = ftell(picture);
            fseek(picture, 0, SEEK_SET);

            // printf("size : %d\n", size);

            // Send file size   
            if((numbytes=send(new_fd, &size, sizeof(int), 0))==-1) {
                perror("server, sending file size : ");
                exit(1);
            }

            // Receive acknowledgement
            if((numbytes=recv(new_fd, ack, sizeof(ack), 0))==-1) {
                perror("server, recv acknowledgement");
                exit(1);
            }
            
            // // Declare sending buffer
            char send_buffer[size];

            // Send file
            fread(send_buffer, 1, sizeof(send_buffer), picture);

            numbytes = send(new_fd, send_buffer, sizeof(send_buffer), 0);
            printf("%d bytes sent\n\n", numbytes);

            // Receive acknowledgement
            if((numbytes=recv(new_fd, ack, sizeof(ack), 0))==-1) {
                perror("server, recv acknowledgement");
                exit(1);
            }
            // printf("Received Acknowledgement %d\n", numbytes);


            fclose(picture);

        }
    }

    closedir(dr);     
    close(new_fd);

    return 0;
}

int main() {

    int sockfd_udp, sockfd_tcp, numbytes_udp, numbytes_tcp;
    struct addrinfo hints, *servinfo_udp, *p_udp, *servinfo_tcp, *p_tcp;
    int yes = 1;
    int rv;

    // UDP Server initialize

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_flags = AI_PASSIVE;
    hints.ai_socktype = SOCK_DGRAM;

    if((rv=getaddrinfo(NULL, PORT, &hints, &servinfo_udp)!=0)) {
        printf("Error : %s\n", gai_strerror(rv));
    }

    for(p_udp=servinfo_udp; p_udp!=NULL; p_udp=p_udp->ai_next) {
        if((sockfd_udp = socket(p_udp->ai_family, p_udp->ai_socktype, p_udp->ai_protocol))==-1) {
            perror("server: socket");
            continue;
        }

        if(setsockopt(sockfd_udp, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))==-1) {
            perror("setsockopt");
            exit(1);
        }

        if(bind(sockfd_udp, p_udp->ai_addr, p_udp->ai_addrlen)==-1) {
            close(sockfd_udp);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo_udp);

    if(p_udp==NULL) {
        fprintf(stderr, "udp server: failed to bind\n");
        exit(1);
    }

    // TCP server initialize
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_flags = AI_PASSIVE;
    hints.ai_socktype = SOCK_STREAM;

    if((rv=getaddrinfo(NULL, PORT, &hints, &servinfo_tcp)!=0)) {
        printf("Error : %s\n", gai_strerror(rv));
    }

    for(p_tcp=servinfo_tcp; p_tcp!=NULL; p_tcp=p_tcp->ai_next) {
        if((sockfd_tcp = socket(p_tcp->ai_family, p_tcp->ai_socktype, p_tcp->ai_protocol))==-1) {
            perror("server: socket");
            continue;
        }

        if(setsockopt(sockfd_tcp, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))==-1) {
            perror("setsockopt");
            exit(1);
        }

        if(bind(sockfd_tcp, p_tcp->ai_addr, p_tcp->ai_addrlen)==-1) {
            close(sockfd_tcp);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo_tcp);

    if(p_tcp==NULL) {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    while(1) {

        // select statement
        if(1) {
            struct sockaddr_storage tcp_client_addr;
            socklen_t tcp_client_addr_size;
            int new_fd = accept(sockfd_tcp, (struct sockaddr*)&tcp_client_addr, &tcp_client_addr_size);
            tcp_job(new_fd, tcp_client_addr, tcp_client_addr_size);
        }
        else {
            // if udp is set
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

            buff[numbytes] = '\0';

            struct hostent *h;

            if ((h=gethostbyname(buff)) == NULL) {  // get the host info
                herror("gethostbyname");
                exit(1);
            }

            char *ipstr = inet_ntoa(*((struct in_addr *)h->h_addr));

            // Send ip back
            if((numbytes=sendto(sockfd_udp, ipstr, strlen(ipstr), 0, (struct sockaddr*)&their_addr, their_addr.ss_len))==-1) {
                perror("client: sendto");
                exit(1);
            }
        }
    }
    


}