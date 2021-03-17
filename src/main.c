#include <workers.h>

void* memory( void* ud, void* ptr, size_t osize, size_t nsize );
void* worker_lua( struct worker *worker );

struct worker *lua_worker = NULL;

int main()
{
	printf("Gasp Alpha\n");
	
	if ( main_worker( (Worker_t)worker_lua ) == 0 )
		return EXIT_SUCCESS;
	
	return EXIT_FAILURE;
}

void* lua_memory( void *ud, void *ptr, size_t osize, size_t nsize )
{
	if ( ptr || nsize )
	{
		int ret;
		ssize_t bytes;
		struct worker_msg *worker_msg, own_msg;
		struct worker_block worker_block;
		struct memory_block memory_block = {0};
		void *own = &own_msg, *ptr = (void*)&(worker_msg);
		
		worker_block.memory_block = &memory_block;
		worker_block.want = nsize;
		memory_block.block = ptr;
		memory_block.bytes = osize;
		
		own_msg.type = WORKER_MSG_ALLOC;
		own_msg.data = &worker_block;
		
		/* While this does create latency it also creates stability */
		ret = wrpipe( lua_worker->all_pipes[PIPE_WR], own, &bytes );
		
		if ( ret != 0 )
		{
			if ( nsize )
				return NULL;
				
			printf( "Panic mode! Aborting everything..." );
			exit( EXIT_FAILURE );
			
			/* This should never be reached but let's have it here just in
			 * case the unexpected occurs */
			return ptr;
		}
		
		while ( (ret = poll_pipe( lua_worker->own_pipes[PIPE_RD] )) )
		{
			ret = rdpipe( lua_worker->own_pipes[PIPE_RD], ptr, &bytes );
			
			if ( ret != 0 )
				return NULL;
			
			if ( ptr != own || worker_msg->type != WORKER_MSG_CONT )
				continue;
		}
		
		return (memory_block.bytes >= nsize) ? memory_block.block : NULL;
	}
	
	return NULL;
}

void* worker_lua( struct worker *worker )
{
	struct worker_msg own_msg = {0};
	void *own = &own_msg;
	ssize_t bytes;
	int ret;
	
	lua_worker = worker;
	
	/* Only way to identify the worker after it died */
	own_msg.type = WORKER_MSG_DIED;
	own_msg.data = worker;
	
	ret = wrpipe( worker->all_pipes[PIPE_WR], own, &bytes );
	
	return worker;
}
