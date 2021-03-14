#include <workers.h>

void* worker_lua( struct worker *worker )
{
	struct worker_msg own = {0};
	
	/* Only way to identify the worker after it died */
	own.type = WORKER_MSG_DIED;
	own.data = worker;
	
	return worker;
}

int main()
{
	int i;
	enum
	{
		WORKER_NULL = 0,
		WORKER_MM,
		WORKER_FOO,
		WORKER_COUNT
	};
	pipe_t pipes[PIPE_COUNT] = {INVALID_PIPE,INVALID_PIPE};
	worker_t workers[WORKER_COUNT] = {{0}}, *worker, *worker_ret[WORKER_COUNT];
	Worker_t Worker[WORKER_COUNT] = { NULL };
	struct pollfd fds = {0};
	void *ptr = NULL;
	
	fds.fd = pipes[PIPE_RD];
	fds.events = POLLIN | POLLERR | POLLHUP | POLLNVAL;
	
	Worker[WORKER_MM] = (Worker_t)mmworker;
	Worker[WORKER_FOO] = (Worker_t)fooworker;
	
//#define JOIN
#ifdef JOIN
	workers[WORKER_MM].join = true;
	workers[WORKER_FOO].join = true;
#endif
	
	printf("Hello World!\n");
	
	if ( open_pipes( pipes ) != 0 )
		goto cleanup;
	
	for ( i = 1; i < WORKER_COUNT; ++i )
	{
		printf( "Creating worker %d...\n", i );
		
		worker = &(workers[i]);
		worker->num = i;
		worker->tid = INVALID_TID;
		worker->all_pipes[PIPE_RD] = pipes[PIPE_RD];
		worker->all_pipes[PIPE_WR] = pipes[PIPE_WR];
		worker->attr_created = false;
		
		if ( open_pipes( worker->own_pipes ) != 0 )
			goto cleanup;
		
		if ( pthread_attr_init( &(worker->attr) ) != 0 )
			goto cleanup;
		
		worker->attr_created = true;
		
		if
		( pthread_create(
				&(worker->tid), &(worker->attr), Worker[i], worker ) != 0
		) goto cleanup;
		
		if ( !(worker->join) && pthread_detach( worker->tid ) != 0 )
			goto cleanup;
	}

	while ( (ret = poll( &fds, 1, 1 )) >= 0 )
	{
		if ( ret == 1 )
		{
			/* Prevent uncaught messages by having them sent to all workers,
			 * they can just ignore what is not relevant to them */
			
			rdpipe( pipes[PIPE_RD], (void*)(&ptr) );
			
			for ( int w = WORKER_MM; w < WORKER_COUNT; ++w )
			{
				wrpipe( worker->own_pipes[PIPE_WR], ptr );
			}
		}
	}
	
	cleanup:
	
	while ( i > WORKER_MM )
	{
		--i;
		worker = &(workers[i]);
		
		printf( "Cleaning worker %d\n", i );
		
		if ( worker->num >= 0 )
			pthread_cancel( worker->tid );
		
		if ( worker->attr_created )
			pthread_attr_destroy( &(worker->attr) );
		
		if ( worker->own_pipes[PIPE_WR] )
			shut_pipes( worker->own_pipes );
		
		worker->attr_created = false;
		worker->mmi_pipes[PIPE_WR] = INVALID_PIPE;
		worker->mmi_pipes[PIPE_RD] = INVALID_PIPE;
		worker->tid = INVALID_TID;
	}
	
	shut_pipes( pipes );
	
	return 0;
}
