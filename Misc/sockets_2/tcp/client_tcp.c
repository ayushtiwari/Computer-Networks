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
#include <dirent.h> 
#include <sys/stat.h>

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

    char ack[]="gotit";
    int no_of_images = 0;

    // Send subdir name
    if((numbytes=send(sockfd, "im1", 4, 0))==-1){
        perror("client, sending subdir name : ");
        exit(1);
    }

    // Get no of images
    if((numbytes=recv(sockfd, &no_of_images, sizeof(no_of_images), 0))==-1) {
        perror("client, getting no_of_images : ");
        exit(1);
    }

    printf("Number of images: %d\n\n", no_of_images);

    struct stat st = {0};

    if (stat("./client_images", &st) == -1) {
        mkdir("./client_images", 0700);
    }
    
    int file_counter = 0;

    while(file_counter < no_of_images) {
        char file_pathname[64];
        // Recieve file pathname
        if((numbytes=recv(sockfd, file_pathname, sizeof(file_pathname), 0))==-1) {
            perror("client, recv file pathname : \n");
            exit(1);
        }

        printf("Receiving %s...\n", file_pathname);

        // Send ack
        if((numbytes=send(sockfd, ack, sizeof(ack), 0))==-1) {
            perror("client, sending acknowledgement\n");
            exit(1);
        }

        // Recieve size
        int size;
        if((numbytes=recv(sockfd, &size, sizeof(int), 0))==-1) {
            perror("client, recv file size : \n");
            exit(1);
        }

        // printf("size : %d\n", size);

        // Send ack
        if((numbytes=send(sockfd, ack, sizeof(ack), 0))==-1) {
            perror("client, sending acknowledgement\n");
            exit(1);
        }

        // // Declare picture array
        char p_array[2*size];

        // Receive file

        if((numbytes=recv(sockfd, p_array, 2*size, 0))==-1) {
            perror("client, recv image : \n");
            exit(1);
        }

        printf("Recieved %d bytes\n", numbytes);

        // Send ack
        if((numbytes=send(sockfd, ack, sizeof(ack), 0))==-1) {
            perror("client, sending acknowledgement\n");
            exit(1);
        }

        // Save file
        char save_file_name[64];
        sprintf(save_file_name, "./client_images/image_%d.jpg", file_counter);

        printf("Saving to %s...\n\n", save_file_name);

        FILE *image;
        image = fopen(save_file_name, "w");

        if(image == NULL) {
            fprintf(stderr, "save file error!\n");
        }

        fwrite(p_array, 1, sizeof(p_array), image);
        fclose(image);

        file_counter++;
    }

  

    

    

    close(sockfd);

    return 0;

}