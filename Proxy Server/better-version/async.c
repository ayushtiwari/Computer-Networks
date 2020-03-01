#include "async.h"

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
    
    n = 1;
    if (setsockopt (s, SOL_SOCKET, SO_KEEPALIVE, (void *) &n, sizeof (n)) < 0)
        fatal ("SO_KEEPALIVE: %s\n", strerror(errno));
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
