#ifndef WORKERS_H
#define WORKERS_H

#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <poll.h>
#include <pthread.h>
#include <string.h>

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
	, WORKER_MSG_ALLOC
	, WORKER_MSG_COUNT
};

#define INVALID_TID -1

struct worker
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
	struct memory_group memory_group;
};

struct worker_block
{
	size_t want;
	struct worker *worker;
	struct memory_block *memory_block;
};

struct shared_block
{
	size_t holders;
	void *block;
};

struct worker_msg
{
	int type;
	struct worker *worker;
	void *data;
};

int new_worker( char const * const name );
#define get_worker( index ) ((index >= 0 && index )
void del_worker( int index );

typedef void * (*Worker_t)( void *arg );

void * worker_mm( struct worker *worker );
void * worker_foo( struct worker *worker );

char const * const get_worker_msg_txt( int msg );

int main_worker( Worker_t run );

#endif
