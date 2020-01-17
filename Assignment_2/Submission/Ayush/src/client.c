#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <fcntl.h>

#define SERVER_HOST "127.0.0.1"
#define SERVER_PORT 1234
#define BUFFLEN 256

char line[BUFFLEN], word[128]="Word ";
struct sockaddr_in server;
int sock, rlen, slen=sizeof(server);

int main() {
    printf("1. create a UDP socket\n");
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if(sock < 0) {
        printf("Error occurred\n");
        exit(0);
    }

    printf("2. fill in server address and port number\n");
    memset((char*) &server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(SERVER_PORT);
    inet_aton(SERVER_HOST, &server.sin_addr);

    char *filename = "files/alpha.txt";

    sendto(sock, filename, strlen(filename), 0, (struct sockaddr*)&server, slen);

    memset(line, 0, BUFFLEN);
    rlen = recvfrom(sock, line, BUFFLEN, 0, (struct sockaddr*)&server, &slen);
    printf("Connection established with [host:port] = [%s:%d]\n\n", inet_ntoa(server.sin_addr), ntohs(server.sin_port));

    if(strcmp(line, "HELLO")!=0) {
        printf("Error Occurred! (First line not \"HELLO\"");
        exit(0);
    }

    // Change stdout to file
    int fd = open("files/beta.txt", O_WRONLY|O_CREAT, 0644);
    if(fd < 0) {
        printf("Error creating file\n");
        exit(0);
    }
    close(1);
    dup(fd);

    printf("%s\n", line);

    int wc=0;

    while(1) {
        sprintf(line, "Word %d", wc++);
        sendto(sock, line, strlen(line), 0, (struct sockaddr*)&server, slen);
        memset(line, 0, BUFFLEN);
        rlen = recvfrom(sock, line, BUFFLEN, 0, (struct sockaddr*)&server, &slen);
        printf("%s\n", line);
        if(strcmp(line, "END")==0) break;
    }
    // Change stdout to terminal
    close(1);
    open("/dev/tty", O_WRONLY);
    
    printf("Transfer Successfull!\n");

    close(sock);
    return 0;
}