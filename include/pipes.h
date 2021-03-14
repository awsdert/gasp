#ifndef PIPES_H
#define PIPES_H

#ifdef _WIN32
#include <namedpipeapi.h>
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
ssize_t rdpipe( pipe_t pipe, void *data, size_t size );
ssize_t wrpipe( pipe_t pipe, void *data, size_t size );

#endif
