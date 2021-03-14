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
	ALL_MSG_NIL = 0
	, ALL_MSG_DIE
	, MM_MSG_NEW
	, MM_MSG_DEL
	, MM_MSG_ALT
	, MM_MSG_INC
	, MM_MSG_DEC
	, MM_MSG_DIE
	, MM_MSG_WAIT
	, MM_MSG_READY
	, MM_MSG_COUNT
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
	pipe_t mmi_pipes[PIPE_COUNT], mmo_pipes[PIPE_COUNT];
	// Will an attempt to join be made?
	bool join;
	// Should an attempt to close member attr be made by main() during cleanup?
	bool attr_created;
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

#endif
