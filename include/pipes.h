#ifndef PIPES_H
#define PIPES_H

#ifdef _WIN32
#include <namedpipeapi.h>
#else
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <string.h>
#endif

#define PIPE_RD 0
#define PIPE_WR 1
#define PIPE_COUNT 2

#ifdef _WIN32
typedef HANDLE pipe_t;
#define INVALID_PIPE NULL
#else
typedef int pipe_t;
#define INVALID_PIPE -1
#endif

int open_pipes( pipe_t *pipes );
void shut_pipes( pipe_t *pipes );
int pipe_err( pipe_t pipe );

/* Will only assume sizeof(void*) */
int poll_pipe( pipe_t pipe );
int rdpipe( pipe_t pipe, void *data, ssize_t *done );
int wrpipe( pipe_t pipe, void *data, ssize_t *done );

#endif
