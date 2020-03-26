#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>

#include <stdarg.h>
#include <assert.h>
#include <sys/socket.h>
#include <sys/stat.h>

#ifndef RSOCKET_H
#define RSOCKET_H

#define R_CONSTANT 100

/* timeout in secs */
#define TIMEOUT 2
#define DROP_P 1


int transmit_count;

/* message type */
typedef struct msg {
    int id;
    int src_port, dest_port;
    enum {R_ACK, R_APP, R_END} type;
    time_t dtime; 
    char payload;
} msg_t;

/* connection type */
typedef struct connection {
    struct sockaddr_in src_addr, dest_addr;
    msg_t send_buffer[R_CONSTANT],
          recv_buffer[R_CONSTANT];
    int send_count,
        recv_in,
        recv_out;
    msg_t *unack_table,
          *recvd_table;
    int id_count;
    int valid;
} conn_t;

/* connections */
conn_t connection[R_CONSTANT];
int port2sock[65563];

msg_t _message;     /* for internal use */

/* Create MRP socket */
int
r_socket (void);

/* Binds the socket to a port*/
int
r_bind (int socket, int port);

/* virtual connect */
int
r_vconnect (int socket, char dst_ip[128], int dst_port);

/* Sends message to their_addr */
ssize_t
r_sendto (int socket, const void *buffer, size_t length);

/* Receive queue operations */
int
enqueue_recv_buffer (int socket, msg_t message);

msg_t
dequeue_recv_buffer (int socket);

int
empty_recv_buffer (int socket);

int
full_recv_buffer (int socket);

/* Receives message */
ssize_t
r_recvfrom (int socket, void *buffer, size_t length);

/* Closes r_socket */
int
r_close (int socket);

int
print_message (msg_t message);

/* timer handler */
void
handler (int sig);

/* signal handler helpers */
int
HandleReceive (void);
int
HandleAPPMsgRecv (int socket, msg_t message);
int
HandleACKMsgRecv (int socket, msg_t message);
int
HandleRetransmit (void);
int
HandleTransmit (void);

int
dropMessage (void);

#endif