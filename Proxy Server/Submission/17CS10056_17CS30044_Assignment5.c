#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h> 
#include <netinet/in.h>
#include <signal.h>
#include <errno.h>

#define MAX_CONN 1024
#define BUFF_SIZE 128

int sps_listener;
struct sockaddr_in sps_addr, ins_addr, cliaddr;

typedef enum {false, true} bool;

typedef struct proxy_registry {
    int sps_cli[MAX_CONN], sps_ins[MAX_CONN];
    struct sockaddr_in cli_addr[MAX_CONN];
    int count;
    int valid[MAX_CONN];
} ProxyReg;

ProxyReg preg;

int handle_connection(int src, int dest) {
    char buff[1024];
    int n;
    n = recv(src, buff, 1024, 0);
    if(n==-1 && (errno==EAGAIN || errno==EWOULDBLOCK)) return 0;
    if(n==-1) return -1;
    send(dest, buff, n, 0);
    return 0;
}

int sps_init(int sps_port, char *ins_ip, int ins_port) {

    for(int i=0; i<MAX_CONN; i++) preg.valid[i]=0;

    preg.count = 0;

    bzero(&ins_addr, sizeof(ins_addr));
    ins_addr.sin_family = AF_INET;
    ins_addr.sin_addr.s_addr = inet_addr(ins_ip);
    ins_addr.sin_port = htons(ins_port);

    if((sps_listener = socket(AF_INET, SOCK_STREAM, 0))<0) {
        fprintf(stderr, "sps_init socket\n");
        exit(1);
    }

    if(fcntl(sps_listener, F_SETFL, O_NONBLOCK) < 0) {
        fprintf(stderr, "sps_init fcntl\n");
        exit(1);
    }

    if((setsockopt(sps_listener, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)) {
        fprintf(stderr, "sps_init setsockopt\n");
        exit(1);
    }

    bzero(&sps_addr, sizeof(sps_addr)); 
    sps_addr.sin_family = AF_INET;
    sps_addr.sin_addr.s_addr = INADDR_ANY;
    sps_addr.sin_port = htons(sps_port);

    if(bind(sps_listener, (const struct sockaddr *)&sps_addr, sizeof(sps_addr)) < 0)  { 
        fprintf(stderr, "sps_init bind\n"); 
        exit(1); 
    }

    if(listen(sps_listener, MAX_CONN) < 0) {
        fprintf(stderr, "sps_init listen\n");
        exit(1);
    }
    
    printf("Proxy running on port %d. Forwarding all connections to %s:%d\n", sps_port, ins_ip, ins_port);
}

int sps_wrap() {
    for(int i = 0 ; i < MAX_CONN ; i++) {
        if(preg.valid[i]==0) continue;
        close(preg.sps_cli[i]);
        close(preg.sps_ins[i]);
    }
    close(sps_listener);

    return 0;
}

int accept_connection() {

    fflush(NULL);

    if(preg.count > MAX_CONN) {
        fprintf(stderr, "MAX_CONN connections reached\n");
        exit(1);
    }

    int index=0;
    while(preg.valid[index]==1) index++;

    socklen_t len = sizeof(preg.cli_addr[preg.count]);
    if((preg.sps_cli[preg.count] = accept(sps_listener, (struct sockaddr *)&preg.cli_addr[index], &len)) < 0) {
        fprintf(stderr, "accept_connection accept\n");
        exit(1);
    }

    printf("Connection accepted from %s:%d\n",
            inet_ntoa(preg.cli_addr[index].sin_addr), 
            (int) ntohs(preg.cli_addr[index].sin_port));

    if( (preg.sps_ins[index] = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "accept_connection socket\n");
        exit(1);
    }

    if(fcntl(preg.sps_ins[index], F_SETFL, O_NONBLOCK) < 0) {
        fprintf(stderr, "accept_connection fcntl\n");
        exit(1);
    }

    connect(preg.sps_ins[index], (const struct sockaddr *)&ins_addr, sizeof(ins_addr));
    preg.valid[index] = 1;
    preg.count++;
}

int main(int argc, char *argv[]) {

    if(argc < 4) {
        fprintf(stderr, "Usage : ./simproxy serverport institute_ip  institute_port\n");
        exit(1);
    }

    sps_init(atoi(argv[1]), argv[2], atoi(argv[3]));
    
    char clientip[MAX_CONN];
    
    fd_set read_fds;
    fd_set write_fds;

    int max_fd;

    signal(SIGPIPE, SIG_IGN);

    for(;;) {
        FD_ZERO(&read_fds);
        FD_ZERO(&write_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        FD_SET(sps_listener, &read_fds);
        max_fd = sps_listener;

        for(int i = 0 ; i < preg.count ; i++) {
            if(preg.valid[i]==0) continue;
            FD_SET(preg.sps_cli[i], &read_fds);
            FD_SET(preg.sps_ins[i], &read_fds);
            FD_SET(preg.sps_cli[i], &write_fds);
            FD_SET(preg.sps_ins[i], &write_fds);
            
            if(max_fd < preg.sps_cli[i]) max_fd = preg.sps_cli[i];
            if(max_fd < preg.sps_ins[i]) max_fd = preg.sps_ins[i];
        }
	

        int ready = -1;
        if((ready = select(max_fd + 1, &read_fds, &write_fds, NULL, NULL)) < 0) {
            fprintf(stderr, "main select\n");
            continue;
        }

        if(FD_ISSET(STDIN_FILENO, &read_fds)) {
            char input[256];
            printf("hello\n");
            scanf("%s", input);
            if(strcmp(input, "exit")==0 && !sps_wrap()) exit(0);
            fprintf(stderr, "wrap up error\n");
            exit(1);
        }

        if(FD_ISSET(sps_listener, &read_fds)) accept_connection();

        for(int i = 0 ; i < MAX_CONN ; i++) {
            if(preg.valid[i]==0) continue;
            if(FD_ISSET(preg.sps_cli[i], &read_fds) && FD_ISSET(preg.sps_ins[i], &write_fds)) {
                if(handle_connection(preg.sps_cli[i], preg.sps_ins[i]) < 0) {
                    close(preg.sps_cli[i]);
                    close(preg.sps_ins[i]);
                    preg.valid[i] = 0;
                    preg.count--;
                }
            }

            else if(FD_ISSET(preg.sps_ins[i], &read_fds) && FD_ISSET(preg.sps_cli[i], &write_fds)) {
                if(handle_connection(preg.sps_ins[i], preg.sps_cli[i]) < 0) {
                    close(preg.sps_cli[i]);
                    close(preg.sps_ins[i]);
                    preg.valid[i] = 0;
                    preg.count--;
                }
            }
        }
    }
}
