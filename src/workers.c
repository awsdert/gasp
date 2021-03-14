#include <workers.h>

void * mmworker( worker_t *worker )
{
	int ret;
	struct pollfd fds = {0};
	pipe_t mm_pipes[PIPE_COUNT] =
	{
		worker->mmi_pipes[PIPE_RD],
		worker->mmo_pipes[PIPE_WR]
	};
	
	fds.fd = mm_pipes[PIPE_RD];
	fds.events = POLLIN | POLLERR | POLLHUP | POLLNVAL;
	
	printf( "Worker %d started\n", worker->num );
	
	while ( (ret = poll( &fds, 1, 1 )) >= 0 )
	{
		if ( ret == 1 )
		{
			struct worker_msg *worker_msg = NULL;
			ssize_t bytes;
			
			bytes = rdpipe(
				mm_pipes[PIPE_RD], &worker_msg, sizeof(struct worker_msg) );
			
			printf( "Worker %d: Read %zd bytes\n", worker->num, bytes );
		}
	}
	
	/* Indicate the thread exited */
	worker->num = -1;
	return worker;
}

void * fooworker( worker_t *worker )
{
	int ret;
	struct pollfd fds = {0};
	pipe_t mm_pipes[PIPE_COUNT] =
	{
		worker->mmo_pipes[PIPE_RD],
		worker->mmi_pipes[PIPE_WR]
	};
	ssize_t bytes;
	struct worker_msg *worker_msg = NULL;
	
	printf( "Worker %d started\n", worker->num );
	
#if 0
	while ( (ret = poll( &fds, 1, 1 )) >= 0 )
	{
		if ( ret == 1 )
		{
			struct worker_msg *worker_msg = NULL;
			ssize_t bytes;
			
			bytes = rdpipe
			(
				mm_pipes[PIPE_RD]
				, &worker_msg
				, sizeof(struct worker_msg)
			);
			
			printf( "Worker %d: Read %zd bytes\n", worker->num, bytes );
		}
	}
#endif
	
	bytes = wrpipe(
		mm_pipes[PIPE_WR], &worker_msg, sizeof(struct worker_msg) );
	
	printf( "Worker %d: Wrote %zd bytes\n", worker->num, bytes );
	
	/* Indicate the thread exited */
	worker->num = -1;
	return worker;
}
