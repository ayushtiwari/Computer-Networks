# Reliable UDP
### Transmission Table


String: 12345678 <br/>
Timeout: 2sec <br/>
   
    prob | avg_t
    ------------
    0.50 | 4.625
    0.45 | 6.625
    0.40 | 6.875
    0.35 | 7
    0.30 | 8.375 
    0.25 | 16.75
    0.20 | 21.75
    0.15 | 26.87
    0.10 | 39.25
    ------------

### Message structure

```c
typedef struct msg {
    int id;
    int src_port, dest_port;
    enum {R_ACK, R_APP, R_END} type;
    time_t dtime; 
    int length;
    char payload;
} msg_t;
```

- id = message id
- src_port = source port
- dest_port = destination port 
- type = type of message 
- dtime = departure time
- payload = character to send

### Connection structure

```c
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
```

- src_addr = source address
- dest_addr = destination address
- send_buffer = sending buffer 
- recv_buffer = receiving buffer (circular queue)
- send_count = send_buffer current full size 
- recv_in, recv_out = for receiving queue operations
- unack_table = unacknowledged message table
- recvd_table = received message table 
- id_count = current id count for this connection 
- valid = is the connection valid

```c
/* connection array */
/* connection[socket] = connection corresponding to socket */
conn_t connection[R_CONSTANT];
```
```c
/* mapping from port to socket */
int port2sock[65563];
```




