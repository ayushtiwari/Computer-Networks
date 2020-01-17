#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <fcntl.h>

#define BUFFLEN 256
#define PORT 1234

char line[BUFFLEN];
struct sockaddr_in me, client;
int sock, rlen, clen=sizeof(client);

int main() {
    printf("1. create a UDP socket\n");
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if(sock < 0) {
        printf("Error occurred\n");
        exit(0);
    }

    printf("2. fill me with server address and port number\n");
    memset((char*)&me, 0, sizeof(me));
    me.sin_family = AF_INET;
    me.sin_port = htons(PORT);
    me.sin_addr.s_addr = htonl(INADDR_ANY);

    printf("3. bind the socket to server IP and PORT\n");

    if(bind(sock, (struct sockaddr*)&me, sizeof(me))<0) {
        printf("Could not bind!\n");
        exit(0);
    }

    printf("4. wait for datagram\n\n");

    memset(line, 0, BUFFLEN);
    printf("UDP server : waiting for datagram\n");
    rlen = recvfrom(sock, line, BUFFLEN, 0, (struct sockaddr*)&client, &clen);
    printf("received a datagram from client [host:port] = [%s:%d]\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port));
    printf("line = %s\n", line);

    // See if file exists
    int fd = open(line, O_RDONLY);
    if(fd < 0) {
        char temp[128]="Not Found : ";
        strcat(temp, line);
        sendto(sock, line, sizeof(line), 0, (struct sockaddr*)&client, clen);
        exit(0);
    }

    // Change stdin to file
    close(0);
    dup(fd);

    printf("File was found\n");
    scanf("%s", line);
    printf("Sending %s\n\n", line);
    sendto(sock, line, sizeof(line), 0, (struct sockaddr*)&client, clen);

    while(1) {
        memset(line, 0, BUFFLEN);
        printf("UDP server : Waiting for datagram\n");
        rlen = recvfrom(sock, line, BUFFLEN, 0, (struct sockaddr*)&client, &clen);
        printf("Received a datagram from client [host:port] = [%s:%d]\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port));
        printf("Client : %s\n", line);

        scanf("%s", line);

        printf("Sending %s\n\n", line);
        sendto(sock, line, sizeof(line), 0, (struct sockaddr*)&client, clen);

        if(strcmp(line, "END")==0) break;
    }

    printf("Transfer Successful!\n");

    close(sock);

    return 0;
}
