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
#define BACKLOG 10

void *get_in_addr(struct sockaddr *sa) {
    if(sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(void) {

    int sockfd, new_fd;
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr;
    socklen_t sin_size;
    int yes = 1;
    char s[INET6_ADDRSTRLEN];
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_flags = AI_PASSIVE;
    hints.ai_socktype = SOCK_STREAM;

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

    if(listen(sockfd, BACKLOG)==-1) {
        perror("listen");
        exit(1);
    }

    
    printf("server waiting for connections...\n");
    while(1) {
        
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr*)&their_addr, &sin_size);
        if(new_fd == -1) {
            perror("accept");
            continue;
        }

        inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr*)&their_addr), s, sizeof(s));

        printf("\nserver got connection from : %s\n", s);
        if(!fork()) {
            int numbytes;
            close(sockfd);

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

            printf("server closed connection with : %s\n\n", s);

            exit(0);
        }

        close(new_fd);
    }

}