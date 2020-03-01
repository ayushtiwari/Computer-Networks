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

#include "async.h"

#define bzero(ptr, size) memset (ptr, 0, size)

#define BUFF_MAX 1024
#define CONN_MAX FD_MAX-1

char *institute_ip = "172.16.2.30";
int institute_port = 8080;

int n_conn = 0;

struct proxy_reg {
    int client_fd, serv_fd;
    char *cli_ip, *serv_ip;
    int cli_port, serv_port;
    char buff[BUFF_MAX];
    /* No. of elements in buff */
    int full;
};

static void
proxy_reg_free (struct proxy_reg *pr)
{
    if (pr->client_fd > 0) {
        cb_free (pr->client_fd, 0);
        cb_free (pr->client_fd, 1);
        close (pr->client_fd);
    }
    if (pr->serv_fd > 0) {
        cb_free (pr->serv_fd, 0);
        cb_free (pr->serv_fd, 1);
        close (pr->serv_fd);
    }

    free (pr->cli_ip);
    free (pr->serv_ip);
    free (pr);
    n_conn--;
}

/**
 * Create a tcp server
 * @returns: file descriptor on success -1 on failure
 */
int
tcpserv (int port)
{
    int s, n;
    struct sockaddr_in sin;

    /* The address of this server */
    bzero(&sin, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons (port);
    sin.sin_addr.s_addr = htonl (INADDR_ANY);

    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        perror("socket");
        return -1;
    }
    /* make address resuable */
    n = 1;
    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char*)&n, sizeof (n)) < 0) {
        perror ("SO_REUSEADDR");
        close(s);
        return -1;
    }

    fcntl(s, F_SETFD, 1);
    /* bind the socket to an address */
    if (bind (s, (struct sockaddr*)&sin, sizeof (sin)) < 0) {
        fprintf(stderr, "TCP port %d: %s\n", port, strerror (errno));
        close (s);
        return -1;
    }
    if (listen (s, 10) < 0) {
        perror ("listen");
        close(s);
        return -1;
    }

    return s;
}


/**
 * Create a TCP connection to host and port.
 * @return: file descriptor on success -1 on error
 */
int
tcpconnect (char *host, int port)
{
    struct hostent *h;
    struct sockaddr_in sa;

    int s;

    /* Get the address of the host to connect from
     * hostname */
    h = gethostbyname (host);
    if (!h) {
        fprintf (stderr, "%s: no such host\n", host);
        return -1;
    }

    /* Create TCP socket */
    s = socket(AF_INET, SOCK_STREAM, 0);

    /* Use bind to set an address and port number for
     * this end of the TCP connection */
    bzero (&sa, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons (0);
    sa.sin_addr.s_addr = htonl (INADDR_ANY);
    if (bind (s, (struct sockaddr*)&sa, sizeof(sa)) < 0) {
        perror ("host");
        close(s);
        return -1;
    }

    /* use h to set the destination address */
    sa.sin_port = htons(port);
    sa.sin_addr = *(struct in_addr*)(*(h->h_addr_list));

    /* connect to the server */
    if (connect (s, (struct sockaddr*)&sa, sizeof(sa)) < 0 && errno != EINPROGRESS) {
        perror("host");
        close(s);
        return -1;
    }

    return s;
}

static void
send_message (void *arg, int sock)
{   
    struct proxy_reg *pr = (struct proxy_reg*)arg;
    int n;
    
    /* if buffer is not empty return */
    if (pr->full == 0)
        return;

    /* write to the socket */
    n = write (sock, pr->buff, pr->full);
    if (n < 0) {
        if (errno == EAGAIN)
            return;
        proxy_reg_free (pr);
        return;
    }

    /* check if all data was written */
    if (n < pr->full) {
        printf ("alpha\n");
        char temp[BUFF_MAX];

        bzero (temp, BUFF_MAX);
        strcpy (temp, pr->buff + n);
        
        bzero (pr->buff, BUFF_MAX);
        strcpy (pr->buff, temp);
        pr->full = pr->full - n;
        return;
    }

    pr->full = 0;
    cb_free (sock, 1);
}

static void
receive_message (void *arg, int sock)
{  
    struct proxy_reg *pr = (struct proxy_reg*)arg;
    int n;

    /* if buffer is full return */
    if (pr->full != 0)
        return;
    
    /* read from socket */
    bzero (pr->buff, BUFF_MAX);
    n = read (sock, pr->buff, BUFF_MAX);
    
    /* check for errors */
    if (n <= 0) {
        if (errno == EAGAIN)
            return;
        proxy_reg_free (pr);
        return;
    }

    pr->full = n;
    cb_add ((sock == pr->client_fd) ? pr->serv_fd : pr->client_fd, 1, send_message, pr);
}

static void
accept_connection (void *arg, int sock) 
{
    struct sockaddr_in cli_addr;
    socklen_t len = sizeof (cli_addr);

    /* accept connection from client */
    int listener = *(int*)arg;
    int cfd = accept (listener, (struct sockaddr*)&cli_addr, &len);
    if (cfd < 0) {
        fatal ("accept: ", strerror (errno));
        exit (1);
    }

    /* make descriptor asynchronous */
    make_async (cfd);
    
    /* fill proxy register */
    struct proxy_reg *pr = (struct proxy_reg*)malloc (sizeof (struct proxy_reg));
    pr->client_fd = cfd;
    pr->serv_fd = tcpconnect (institute_ip, institute_port);
    make_async (pr->serv_fd);

    pr->cli_ip = strdup (inet_ntoa (cli_addr.sin_addr));
    pr->serv_ip = strdup (institute_ip);
    pr->cli_port = ntohs (cli_addr.sin_port);
    pr->serv_port = institute_port;
    pr->full = 0;

    printf ("Connection accepted from %s:%d\n", pr->cli_ip, pr->cli_port);
    n_conn ++;

    cb_add (pr->client_fd, 0, receive_message, pr);
    cb_add (pr->serv_fd, 0, receive_message, pr);

}

int main (int argc, char *argv[])
{   
    /* Writing to an unconnected socket will cause a process to receive
     * a SIGPIPE signal.  We don't want to die if this happens, so we
     * ignore SIGPIPE.  */
    signal (SIGPIPE, SIG_IGN);

    int sockfd = tcpserv(4567);
    printf ("Proxy server running on port %d\n", 4567);
    make_async (sockfd);
    int fd = sockfd;
    cb_add (sockfd, 0, accept_connection, &fd);

    while (1)
        cb_check();
}