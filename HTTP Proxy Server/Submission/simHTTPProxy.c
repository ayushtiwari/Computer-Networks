/* http proxy server */
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

#ifndef bzero
#define bzero(ptr, size) memset (ptr, 0, size)
#endif

#define DEBUG 0
#define LOG 0
#define LOG_FILE "logger.txt"

#define BUFF_MAX 256
#define REQUEST_MAX 2048

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_RESET   "\x1b[0m"

#define FD_MAX 1024
#define CONN_MAX (FD_MAX-3)/2

#define SERVER_CAPACITY 32768

int n_conn = 0;
FILE *logger;

int total_conn_handled=0;

/* Proxy Registry */
struct proxy_reg {
    int id;
    int client_fd, serv_fd;
    char *cli_ip, *serv_ip;
    int cli_port, serv_port;
    char buff[BUFF_MAX];
    char request[REQUEST_MAX];
    struct timeval start, end;
    /* No. of elements in buff */
    int full;
};

struct proxy_reg p_register[SERVER_CAPACITY];
int p_register_index = 0;

/* http request structure */
struct http_request {
    char type[64];
    char host[128];
    char url[128];
    char path[128];
    int port;
};

/* Callback to make when a file descriptor is ready */
struct cb {
    void (*cb_fn) (void *, int);            /* Function to call */
    void *cb_arg;                           /* Argument to pass */
};
static struct cb rcb[FD_MAX], wcb[FD_MAX];  /* Per fd callbacks */
static fd_set rfds, wfds;                   /* Bitmap of cb's in use */

/* Callback functions */
void
cb_add (int fd, int write, void (*fn)(void*, int), void *arg);
void
cb_free (int fd, int write);
void
cb_check (void);
void
make_async (int s);
void *
xrealloc (void *p, size_t size);
void
fatal (const char *msg, ...);

void
cb_add (int fd, int write, void (*fn)(void*, int), void *arg)
{
    struct cb *c;

    assert (fd >= 0 && fd <= FD_MAX);
    c = &(write ? wcb : rcb)[fd];
    c->cb_fn = fn;
    c->cb_arg = arg;
    FD_SET (fd, write ? &wfds : &rfds);
}

void
cb_free (int fd, int write)
{
    assert (fd >= 0 && fd <= FD_MAX);
    FD_CLR (fd, write ? &wfds : &rfds);
}

void
cb_check (void)
{
    fd_set trfds, twfds;
    int i, n;

    /* Call select. Since fd_sets are both input and
     * output arguments we must copy them first */
    trfds = rfds;
    twfds = wfds;
    n = select (FD_MAX, &trfds, &twfds, NULL, NULL);
    if (n < 0) {
        fatal("select: %s\n", strerror(errno));
    }

    /* Loop through and make callbacks for all ready file descriptors */
    for (i=0; n && i<FD_MAX; i++) {
        if (FD_ISSET (i, &trfds)) {
            n--;
            if (FD_ISSET (i, &rfds))
                rcb[i].cb_fn (rcb[i].cb_arg, i);
        }
        if (FD_ISSET (i, &twfds)) {
            n--;
            if (FD_ISSET (i, &wfds))
                wcb[i].cb_fn (wcb[i].cb_arg, i);
        }
    }
}

void
make_async (int s)
{
    int n;

    /* Make file descriptor non blocking */
    if ((n = fcntl (s, F_GETFL)) < 0 || fcntl (s, F_SETFL, n | O_NONBLOCK) < 0)
        fatal ("O_NONBLOCK: %s\n", strerror(errno));

    struct stat statbuf;
    fstat(s, &statbuf);
    if (S_ISSOCK(statbuf.st_mode)) {
        n = 1;
        if (setsockopt (s, SOL_SOCKET, SO_KEEPALIVE, (void *) &n, sizeof (n)) < 0)
            fatal ("SO_KEEPALIVE: %s\n", strerror(errno));
        
        #if SMALL_LIMITS
        #if defined (SO_RCVBUF) && defined (SO_SNDBUF)
        /* Make sure this really is a stream socket (like TCP).  Code using
        * datagram sockets will simply fail miserably if it can never
        * transmit a packet larger than 4 bytes. */
        {
            int sn = sizeof (n);
            if (getsockopt (s, SOL_SOCKET, SO_TYPE, (char *)&n, &sn) < 0
                || n != SOCK_STREAM)
            return;
        }

        n = 4;
        if (setsockopt (s, SOL_SOCKET, SO_RCVBUF, (void *)&n, sizeof (n)) < 0)
            return;
        if (setsockopt (s, SOL_SOCKET, SO_SNDBUF, (void *)&n, sizeof (n)) < 0)
            fatal ("SO_SNDBUF: %s\n", strerror (errno));
        #else /* !SO_RCVBUF || !SO_SNDBUF */
        #error "Need SO_RCVBUF/SO_SNDBUF for SMALL_LIMITS"
        #endif /* SO_RCVBUF && SO_SNDBUF */
        #endif /* SMALL_LIMITS */
    }
}

void *
xrealloc (void *p, size_t size)
{
    p = realloc (p, size);
    if (size && !p)
        fatal ("out of memory\n");
    return p;
}

void
fatal (const char *msg, ...)
{
    va_list ap;

    fprintf (stderr, "fatal: ");
    va_start (ap, msg);
    vfprintf (stderr, msg, ap);
    va_end (ap);
    exit(1);
}

/* clear a connection */
static void
proxy_reg_free (struct proxy_reg *pr)
{
    if (pr==NULL) return;
    
    gettimeofday (&(pr->end), NULL);
    if (p_register_index >= SERVER_CAPACITY) p_register_index = 0;
    memcpy (p_register + p_register_index, pr, sizeof (struct proxy_reg));
    p_register_index ++;
    if (pr->client_fd > 0) {
        cb_free (pr->client_fd, 0);
        cb_free (pr->client_fd, 1);
        close (pr->client_fd);
    }
    if (pr->serv_fd > 0 && pr->serv_fd <= FD_MAX) {
        cb_free (pr->serv_fd, 0);
        cb_free (pr->serv_fd, 1);
        close (pr->serv_fd);
    }
    if (pr->cli_ip!=NULL)
        free (pr->cli_ip);
    if (pr->serv_ip!=NULL)
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
tcpconnect (char *host, int port, char **ip_addr)
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
    make_async (s);

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

    *ip_addr = strdup(inet_ntoa (sa.sin_addr));

    return s;
}

static void
receive_message (void *arg, int sock);
static void
send_message (void *arg, int sock);
static void
method_not_allowed (void *arg, int sock);

static void
method_not_allowed (void *arg, int sock)
{
    printf ("alpha");
    struct proxy_reg *pr = (struct proxy_reg*)arg;
    int n;
    
    /* if buffer is not empty return */
    if (pr->full == 0)
        return;

    /* write to the socket */
    n = write (sock, pr->buff, pr->full);
    printf ("method not allowed %d\n", n);
    if (n < 0) {
        if (errno == EAGAIN)
            return;
        proxy_reg_free (pr);
        return;
    }

    /* check if all data was written */
    if (n < pr->full) {
        char temp[BUFF_MAX];

        bzero (temp, BUFF_MAX);
        strcpy (temp, pr->buff + n);
        
        bzero (pr->buff, BUFF_MAX);
        strcpy (pr->buff, temp);
        pr->full = pr->full - n;
        return;
    }
    pr->full = 0;
    proxy_reg_free (pr);
}

/* send message from one socket to another */
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
    cb_add (sock==pr->client_fd ? pr->serv_fd : pr->client_fd, 0, receive_message, pr);
    cb_free (sock, 1);
}

/**
 * Function to relay message from client to server
 * and vice versa
 */
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
    cb_free (sock, 0);
}

/* parsing function */
struct http_request 
parse (char *request, int *flag) {

    struct http_request hr;
    char url[256];
    int j = 0;
    int i;

    for (i=0; i<15 && request[i] && request[i]!=' '; i++)
        hr.type[i] = request[i];
    hr.type[i] = '\0';
    
    i++;
    for (; request[i] && request[i]==' '; i++);
    for (; request[i] && request[i]!=' '; i++)
        url[j++] = request[i];
    url[j] = '\0';

    int k = 0, l = 0;
    if (j > 4 && strncmp (url, "http://", 7)==0) {
        while (url[k] && url[k]!=':') k++;
        k+=3;
        while (k < 256 && url[k] && url[k]!=':' && url[k]!='/' && url[k]!=' ') hr.host[l++] = url[k++];
    } else {
        while (k < 256 && url[k] && url[k]!=':' && url[k]!=' ') hr.host[l++] = url[k++];
    }
    hr.host[l] = '\0';
    l = 0;
    if (url[k]==':') {
        k++;
        char temp[64];
        for (; k < 256 && url[k] && url[k]!=' '; k++) temp[l++] = url[k];
        temp[l]='\0';
        hr.port = atoi (temp);
    } else {
        hr.port = 80;
    }
    strcpy (hr.url, url);

    int m=0;
    while (url[m] && url[m]!='/') m++;
    m++;
    while (url[m] && url[m]!='/') m++;
    m++;
    while (url[m] && url[m]!='/') m++;
    m++;
    strcpy (hr.path, "/");
    strcat (hr.path, url+m);
    return hr;

}

/* 
 * Function to establish connection after reading
 * first http request from client
 */
static void
initiate_connection (void *arg, int sock)
{
    struct proxy_reg *pr = (struct proxy_reg*)arg;
    /* Peek at the first request */
    bzero (pr->request, REQUEST_MAX);
    int n = recv (pr->client_fd, pr->request, BUFF_MAX, MSG_PEEK);
    /* 
     * An http request is approximately 200 bytes long
     */
    if (n <= 0) {
        if (n==0)
            return;
        if (errno == EAGAIN)
            return;
        printf ("Error");
        proxy_reg_free (pr);
        return;
    }
    /* Most Request Headers are around 200 bytes long */
    if (DEBUG)
        printf ("%s\n", pr->request);
    if (strstr (pr->request, "\r\n")==NULL) 
        return;
    
    /* I should have checked for \r\n\r\n but */
    /* it did not seem to work */
    /*********************************************************/
    /* if (strstr (pr->request, "\r\n\r\n") == NULL) return; */
    /*********************************************************/

    int error = 0;
    struct http_request hr = parse (pr->request, &error);
    if (error) {
        proxy_reg_free (pr);
        return;
    }
    int sfd;
    /* We only accept GET and POST requests */
    if (strcmp (hr.type, "GET")!=0 && strcmp (hr.type, "POST")!=0 ) {
        printf ("%sConnection rejected from %s:%d %s\n", 
                ANSI_COLOR_RED, pr->cli_ip, pr->cli_port, ANSI_COLOR_RESET);
        printf ("%s Request Not Supported\n", hr.type);
        char *reply =  "HTTP/1.1 450 Method Not Allowed\n\r\n\r\n";
        strcpy (pr->buff, reply);
        pr->full = strlen (reply);
        cb_add (pr->client_fd, 1, method_not_allowed, pr);
        cb_free (pr->client_fd, 0);
        return;
    } else {
        printf ("%sConnection accepted from %s:%d %s\n", 
                ANSI_COLOR_GREEN, pr->cli_ip, pr->cli_port, ANSI_COLOR_RESET);
        printf ("%s %s, Host: %s, Path: %s\n", hr.type, hr.url, hr.host, hr.path);
        sfd = tcpconnect (hr.host, hr.port, &(pr->serv_ip));
    }
    
    if (sfd < 0) {
        cb_free (pr->client_fd, 0);
        close (pr->client_fd);
        free (pr->cli_ip);
        free (pr);
        return;
    }
    pr->serv_fd = sfd;
    pr->serv_port = hr.port;

    cb_add (pr->client_fd, 0, receive_message, pr);
    cb_add (pr->serv_fd, 0, receive_message, pr);
}

/* Accept and start serving client */
static void
accept_connection (void *arg, int sock) 
{   
    if (n_conn >= CONN_MAX) {
        printf ("Max connections Reached, Try again later.\n");
        return;
    }

    struct sockaddr_in cli_addr;
    socklen_t len = sizeof (cli_addr);

    /* accept connection from client */
    int listener = *(int*)arg;
    int cfd = accept (listener, (struct sockaddr*)&cli_addr, &len);
    if (cfd < 0) {
        fatal ("accept: ", strerror (errno));
        exit (1);
    }

    n_conn ++;
    /* make descriptor asynchronous */
    make_async (cfd);
    
    /* fill proxy register */
    struct proxy_reg *pr = (struct proxy_reg*)malloc (sizeof (struct proxy_reg));
    pr->id = total_conn_handled++;
    gettimeofday (&(pr->start), NULL);
    pr->client_fd = cfd;
    
    pr->cli_ip = strdup (inet_ntoa (cli_addr.sin_addr));
    pr->cli_port = ntohs (cli_addr.sin_port);
    pr->serv_ip = NULL;
    pr->full = 0;

    cb_add (cfd, 0, initiate_connection, pr);
}

/** 
 * handler for exit command
 */
static void
exit_handler (void *arg, int fd)
{   
    char buff[BUFF_MAX];
    int n;
    /* read from fd */
    bzero (buff, BUFF_MAX);
    n = read (fd, buff, BUFF_MAX);
    
    /* check for errors */
    if (n <= 0) {
        if (n == 0 || errno == EAGAIN)
            return;
        fatal ("exit handler", strerror (errno));
        exit (1);
    }
    if (strcmp (buff, "exit\n")==0)
        exit (0);
}

int main (int argc, char *argv[])
{  
    fflush (stdin);
    if (argc != 2) {
        printf ("Usage %s <port>\n", argv[0]);
        exit (1);
    }
    int port = atoi (argv[1]);
    /* Writing to an unconnected socket will cause a process to receive
     * a SIGPIPE signal.  We don't want to die if this happens, so we
     * ignore SIGPIPE.  */
    signal (SIGPIPE, SIG_IGN);

    if (LOG)
        logger = fopen (LOG_FILE, "w+");

    /* install standard input handler */
    make_async (0);
    cb_add (0, 0, exit_handler, NULL);

    int sockfd = tcpserv(port);
    make_async (sockfd);

    printf ("Proxy server running on port %d\n", port);
    int fd = sockfd;
    cb_add (sockfd, 0, accept_connection, &fd);

    while (1)
        cb_check();
}