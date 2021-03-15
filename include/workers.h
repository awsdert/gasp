#ifndef WORKERS_H
#define WORKERS_H

#define _GNU_SOURCE
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <poll.h>
#include <pthread.h>

#include "memory.h"
#include "pipes.h"

enum
{
	WORKER_MSG_NONE = 0
	, WORKER_MSG_EXIT
	, WORKER_MSG_TERM
	, WORKER_MSG_DIED
	, WORKER_MSG_WAIT
	, WORKER_MSG_CONT
	, WORKER_MSG_MEM_NEW
	, WORKER_MSG_MEM_DEL
	, WORKER_MSG_MEM_ALT
	, WORKER_MSG_MEM_INC
	, WORKER_MSG_MEM_DEC
	, WORKER_MSG_COUNT
};

struct shared_block
{
	size_t want;
	struct memory_block *memory_block;
};

struct worker_msg
{
	int type;
	void *data;
};

#define INVALID_TID -1

typedef struct worker
{
	// Thread reference
	int num;
	// Thread ID
	pthread_t tid;
	// Memory Manager
	pipe_t all_pipes[PIPE_COUNT], own_pipes[PIPE_COUNT];
	// Should an attempt to close member attr be made by main() during cleanup?
	int init_attr_ret, open_pipes_ret, create_thread_ret;
	// Attributes
	pthread_attr_t attr;
} worker_t;

int new_worker( char const * const name );
#define get_worker( index ) ((index >= 0 && index )
void del_worker( int index );

typedef void * (*Worker_t)( void *arg );

void * worker_mm( worker_t *worker );
void * worker_foo( worker_t *worker );

char const * const get_worker_msg_txt( int msg );

int main_worker( Worker_t run );

#endif
