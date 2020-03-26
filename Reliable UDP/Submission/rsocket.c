#include "rsocket.h"

int 
dropMessage(void)
{
    float x = (float)rand()/(float)(RAND_MAX);
    if (x > DROP_P)
        return 1;
    return 0;
}

void
handler (int sig)
{
    HandleReceive ();
    HandleRetransmit ();
    HandleTransmit ();
}

int
HandleReceive (void)
{   
    for (int i=0; i<R_CONSTANT; i++) {
        if (connection[i].valid && !full_recv_buffer (i)) {
            
            struct sockaddr *destination = (struct sockaddr*)(&(connection[i].dest_addr));
            socklen_t addrlen = sizeof (connection[i].dest_addr);
            msg_t message;
            int msg_size = sizeof (msg_t);
            int n = recvfrom (i, &message, msg_size, MSG_DONTWAIT, destination, &addrlen);
            if (n < msg_size) {
                continue;
            } else {
                int p = dropMessage();
                if (p) 
                    return 0;
            }

            if (message.type == R_APP)
                HandleAPPMsgRecv (i, message);
            else if (message.type == R_ACK)
                HandleACKMsgRecv (i, message);
        }
    }
    return 0;
}

int 
r_send_msg (int socket, msg_t message) {

    message.dest_port = ntohs (connection[socket].dest_addr.sin_port);
    message.src_port = ntohs (connection[socket].src_addr.sin_port);

    int n = sendto (socket, &message, sizeof (msg_t), 0,
            (struct sockaddr*)(&(connection[socket].dest_addr)),
            sizeof (connection[socket].dest_addr));
    
    return n;
}

int
HandleAPPMsgRecv (int socket, msg_t message)
{
    if (connection[socket].recvd_table[message.id].id != 0) {
                message.type = R_ACK;
                r_send_msg (socket, message);
                return 0;
            }
    connection[socket].recvd_table[message.id] = message;
    message.type = R_ACK;
    enqueue_recv_buffer (socket, message);
    r_send_msg (socket, message);
    return 0;
}

int
HandleACKMsgRecv (int socket, msg_t message)
{
    for (int i=0; i<R_CONSTANT; i++) {
        if (connection[socket].unack_table[i].id == message.id) {
            connection[socket].unack_table[i].id = 0;
            break;
        }
    }
    return 0;
}

int
HandleRetransmit (void)
{
    // int flag = 0;
    for (int i=0; i<R_CONSTANT; i++) {
        if (connection[i].valid) {
            
            for (int j=0; j<R_CONSTANT; j++) {
                if (connection[i].unack_table[j].id != 0) {
                    time_t curr_time = time (NULL);
                    if (curr_time - connection[i].unack_table[j].dtime > TIMEOUT) {
                        // flag=1;
                        connection[i].unack_table[j].dtime = curr_time;
                        r_send_msg (i, connection[i].unack_table[j]);
                        transmit_count++;
                    }
                }
            }
            
        }
    }
    // if (flag==1)
    //     printf ("transmit_count = %d\n", transmit_count);
    
    return 0;
}

int
HandleTransmit (void)
{
    // int flag=0;
    for (int i=0; i<R_CONSTANT; i++) {
        if (connection[i].valid) {
            for (int j=0; j<connection[i].send_count; j++) {
                // flag = 1;
                msg_t message = connection[i].send_buffer[j];
                message.dtime = time (NULL);
                r_send_msg (i, message);
                transmit_count++;
                int k=0;
                while (connection[i].unack_table[k].id!=0) k++;
                connection[i].unack_table[k] = message;
            }
            connection[i].send_count = 0;
        }
    }

    return 0;
}

int
r_socket (void)
{   
    srand (time (NULL));
    int sock;
    if ((sock = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        perror ("socket");
        exit (1);
    }

    bzero (&connection, sizeof (msg_t));
    connection[sock].send_count = 0;
    connection[sock].recv_out = 0;
    connection[sock].recv_in = 1;
    connection[sock].id_count = 0;
    connection[sock].valid = 1;

    connection[sock].recvd_table = (msg_t*)malloc (sizeof (msg_t)*R_CONSTANT);
    connection[sock].unack_table = (msg_t*)malloc (sizeof (msg_t)*R_CONSTANT);

    signal (SIGALRM, handler);

    struct itimerval timer;
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = 100000;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 10000;

    setitimer (ITIMER_REAL, &timer, NULL);

    return sock;
}


int
r_bind (int socket, int port)
{   

    port2sock[port] = socket;

    connection[socket].src_addr.sin_family = AF_INET;
    connection[socket].src_addr.sin_port = htons (port);
    connection[socket].src_addr.sin_addr.s_addr = htonl (INADDR_ANY);

    if (bind (socket, (struct sockaddr*)&connection[socket].src_addr, sizeof (connection[socket].src_addr)) < 0) {
        perror ("bind");
        exit (1);
    }

    int n = 1;
    if (setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, (char*)&n, sizeof (n)) < 0) {
        perror ("SO_REUSEADDR");
        exit (1);
    }

    return 0;
}

int
r_vconnect (int socket, char dst_ip[128], int dst_port)
{
    connection[socket].dest_addr.sin_family = AF_INET;
    connection[socket].dest_addr.sin_port = htons (dst_port);
    
    struct hostent *h;
    h = gethostbyname (dst_ip);
    if (!h) {
        fprintf (stderr, "%s: no such host\n", dst_ip);
        exit (1);
    }
    connection[socket].dest_addr.sin_addr = *(struct in_addr*)(*(h->h_addr_list));

    printf ("virtual connection established\n");
    printf ("connection[%d].dest_addr = %s:%d\n\n", socket, inet_ntoa (connection[socket].dest_addr.sin_addr), ntohs (connection[socket].dest_addr.sin_port));
    /*
        TODO: Establish connection
    */
   return 0;
}

ssize_t
r_sendto (int socket, const void *buffer, size_t length)
{   
    assert (length < R_CONSTANT);

    int i=0;
    do {
        i=0;
        while (connection[socket].unack_table[i].id == 0) i++;
    } while (i==R_CONSTANT);

    for (int i=0; i<length; i++) {
        while (connection[socket].send_count == R_CONSTANT);

        msg_t message;
        message.src_port = ntohs (connection[socket].src_addr.sin_port);
        message.dest_port = ntohs (connection[socket].dest_addr.sin_port);
        message.id = ++(connection[socket].id_count);
        message.type = R_APP;
        message.dtime = 0;
        message.payload = ((char*)buffer)[i];
        
        connection[socket].send_buffer[connection[socket].send_count] = message;
        connection[socket].send_count ++;
    }

    return length;
}

int
enqueue_recv_buffer (int socket, msg_t message)
{
    connection[socket].recv_buffer[connection[socket].recv_in] = message;
    connection[socket].recv_in = (connection[socket].recv_in + 1)%R_CONSTANT;

    return 0;
}

msg_t
dequeue_recv_buffer (int socket)
{   
    msg_t message = connection[socket].recv_buffer[(connection[socket].recv_out+1)%R_CONSTANT];
    connection[socket].recv_out = (connection[socket].recv_out + 1)%R_CONSTANT; 

    return message;
}

int
empty_recv_buffer (int socket)
{
    return ((connection[socket].recv_out + 1)%R_CONSTANT == connection[socket].recv_in);
}

int
full_recv_buffer (int socket)
{
    return ((connection[socket].recv_in + 1)%R_CONSTANT == connection[socket].recv_out);
}

ssize_t
r_recvfrom (int socket, void *buffer, size_t length)
{   
    
    for (int i=0; i<length; i++) {
        while (empty_recv_buffer (socket));

        msg_t message = dequeue_recv_buffer (socket);
        ((char*)buffer)[i] = message.payload;
    }

    return length;
}

int
print_message (msg_t message)
{   
    printf ("id: %d\n", message.id);
    if (message.type == R_APP)
        printf ("type: R_APP\n");
    else if (message.type == R_ACK)
        printf ("type: R_ACK\n");
    printf ("src_port: %d, dest_port: %d\n", message.src_port, message.dest_port);
    printf ("content: %c\n", message.payload);
    printf ("depart_time: %ld\n\n", message.dtime);

    return 0;
}

int
r_close (int socket)
{
    free (connection[socket].unack_table);
    free (connection[socket].recvd_table);
    close (socket);
    return 0;
}
