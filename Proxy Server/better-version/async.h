#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <sys/socket.h>

#pragma once

#define FD_MAX 1024

/* Callback to make when a file descriptor is ready */
struct cb {
    void (*cb_fn) (void *, int);     /* Function to call */
    void *cb_arg;               /* Argument to pass */
};
static struct cb rcb[FD_MAX], wcb[FD_MAX];  /* Per fd callbacks */
static fd_set rfds, wfds;                   /* Bitmap of cb's in use */

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