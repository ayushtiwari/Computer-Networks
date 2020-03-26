#include <stdio.h>

#include "rsocket.h"


int
main (int argc, char *argv[])
{
    char buff[128];
    bzero (buff, 128);
    printf ("Enter string: ");
    scanf ("%s", buff);
    printf ("\n-------------------\n");
    int sock = r_socket ();
    r_bind (sock, 50000 + 1056);
    r_vconnect (sock, "localhost", 50000 + 2*1056);
    printf ("Sending %s\n\n", buff);
    r_sendto (sock, buff, strlen (buff));
    while (1);
    r_close (sock);
}