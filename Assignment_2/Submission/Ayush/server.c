#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

#define BUFFLEN 256
#define PORT 1234

char line[BUFFLEN];
struct sockaddr_in me, client;
int sock, rlen, clen=sizeof(client);

int get_next_word(FILE *fp, char str[]) {
    if(fp==NULL) return 0;
    char c=0;
    int index=0;

    while((c = fgetc(fp))!='\n' && c!=' ') {
        str[index]=c;
        index++;
    }

    str[index]=0;
    return 1;
}

int main() {
    printf("1. create a UDP socket\n");
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

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

    printf("4. wait for datagram\n");

    memset(line, 0, BUFFLEN);
    printf("UDP server : waiting for datagram\n");
    rlen = recvfrom(sock, line, BUFFLEN, 0, (struct sockaddr*)&client, &clen);
    printf("received a datagram from client [host:port] = [%s:%d]\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port));
    printf("line = %s\n", line);

    FILE *fp = fopen(line, "r");
    if(fp==NULL) {
        char temp[128]="Not Found : ";
        strcat(temp, line);
        sendto(sock, line, sizeof(line), 0, (struct sockaddr*)&client, clen);
        exit(0);
    }

    get_next_word(fp, line);
    printf("File was found\n");
    printf("Sending %s\n\n", line);
    sendto(sock, line, sizeof(line), 0, (struct sockaddr*)&client, clen);

    while(1) {
        memset(line, 0, BUFFLEN);
        printf("UDP server : Waiting for datagram\n");
        rlen = recvfrom(sock, line, BUFFLEN, 0, (struct sockaddr*)&client, &clen);
        printf("Received a datagram from client [host:port] = [%s:%d]\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port));
        printf("Client : %s\n", line);

        get_next_word(fp, line);

        printf("Sending %s\n\n", line);
        sendto(sock, line, sizeof(line), 0, (struct sockaddr*)&client, clen);

        if(strcmp(line, "END")==0) break;
    }

    return 0;
}
