#include <workers.h>

void* worker_lua( struct worker *worker )
{
	struct worker_msg own = {0};
	void *ptr = &own;
	
	/* Only way to identify the worker after it died */
	own.type = WORKER_MSG_DIED;
	own.data = worker;
	
	wrpipe( worker->all_pipes[PIPE_WR], (void*)(&ptr) );
	
	return worker;
}

int main()
{
	printf("Gasp Alpha\n");
	
	if ( main_worker( (Worker_t)worker_lua ) == 0 )
		return EXIT_SUCCESS;
	
	return EXIT_FAILURE;
}
