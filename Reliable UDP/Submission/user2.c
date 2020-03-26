#include <stdio.h>

#include "rsocket.h"


int
main (int argc, char *argv[])
{
    int sock = r_socket ();
    r_bind (sock, 50000 + 2*1056);
    r_vconnect (sock, "localhost", 50000 + 1056);
    char buff[128];
    while (1) {
        bzero (buff, 128);
        r_recvfrom (sock, buff, 1);
        write (1, buff, 1);
    }

    while (1);
    r_close (sock);
}