#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/ip.h>

#define SERVER_HOST "127.0.0.1"
#define SERVER_PORT 1234
#define BUFFLEN 256

char line[BUFFLEN], word[128]="Word ";
struct sockaddr_in server;
int sock, rlen, slen=sizeof(server);

int main() {
    printf("1. create a UDP socket\n");
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    printf("2. fill in server address and port number\n");
    memset((char*) &server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(SERVER_PORT);
    inet_aton(SERVER_HOST, &server.sin_addr);

    char *filename = "alpha.txt";

    printf("Get FILE \"%s\"\n", filename);
    sendto(sock, filename, strlen(filename), 0, (struct sockaddr*)&server, slen);

    memset(line, 0, BUFFLEN);
    // printf("Waiting for server\n");
    rlen = recvfrom(sock, line, BUFFLEN, 0, (struct sockaddr*)&server, &slen);
    printf("Server : %s\n\n", line);

    int wc=0;

    while(1) {
        sprintf(line, "Word %d", wc++);
        printf("Get %s \n", line);
        sendto(sock, line, strlen(line), 0, (struct sockaddr*)&server, slen);
        memset(line, 0, BUFFLEN);
        // printf("Waiting for server\n");
        rlen = recvfrom(sock, line, BUFFLEN, 0, (struct sockaddr*)&server, &slen);
        printf("Server : %s\n\n", line);
        if(strcmp(line, "END")==0) break;
    }
    
    close(sock);
    return 0;
}