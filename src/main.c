#include <workers.h>

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
	
	Worker[WORKER_MM] = (Worker_t)mmworker;
	Worker[WORKER_FOO] = (Worker_t)fooworker;
	
#define JOIN
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
		worker->mmi_pipes[PIPE_RD] = pipes[PIPE_RD];
		worker->mmi_pipes[PIPE_WR] = pipes[PIPE_WR];
		worker->attr_created = false;
		
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

#ifdef JOIN
	pthread_join( workers[WORKER_MM].tid, (void**)&(worker_ret[WORKER_MM]) );
	pthread_join( workers[WORKER_FOO].tid, (void**)&(worker_ret[WORKER_FOO]) );
#endif
	
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
		
		worker->attr_created = false;
		worker->mmi_pipes[PIPE_WR] = INVALID_PIPE;
		worker->mmi_pipes[PIPE_RD] = INVALID_PIPE;
		worker->tid = INVALID_TID;
	}
	
	shut_pipes( pipes );
	
	return 0;
}
