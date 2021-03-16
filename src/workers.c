#include <workers.h>

int _del_worker( struct worker *worker )
{	
	if ( worker->open_pipes_ret == 0 )
	{
		if ( worker->init_attr_ret == 0 )
		{
			if ( worker->create_thread_ret == 0 )
			{
				int ret;
				ssize_t bytes;
				struct worker_msg own_msg = {0};
				void *own = &own_msg, *ptr;
				
				own_msg.type = WORKER_MSG_TERM;
				own_msg.data = worker;
				
				ret = wrpipe( worker->own_pipes[PIPE_WR], own, &bytes );
				
				if ( ret != 0 )
				{
					printf
					(
						"Error 0x%08X (%d) '%s', aborting gasp..."
						, ret, ret, strerror(ret)
					);
					exit( EXIT_FAILURE );
					return ret;
				}
				
				pthread_join( worker->tid, &ptr );
				
				pthread_cancel( worker->tid );
			}
			
			pthread_attr_destroy( &(worker->attr) );
		}
		
		shut_pipes( worker->own_pipes );
	}
		
	worker->num = -1;
	worker->tid = INVALID_TID;
	worker->all_pipes[PIPE_RD] = INVALID_PIPE;
	worker->all_pipes[PIPE_WR] = INVALID_PIPE;
	worker->own_pipes[PIPE_RD] = INVALID_PIPE;
	worker->own_pipes[PIPE_WR] = INVALID_PIPE;
}

int _new_worker( int num, struct worker *worker, int *pipes, Worker_t run )
{
	int ret = 0;
	
	worker->num = num;
	worker->tid = INVALID_TID;
	worker->all_pipes[PIPE_RD] = pipes[PIPE_RD];
	worker->all_pipes[PIPE_WR] = pipes[PIPE_WR];
	worker->open_pipes_ret = open_pipes( worker->own_pipes );
	worker->init_attr_ret = -1;
	worker->create_thread_ret = -1;
	
	if ( worker->open_pipes_ret == 0 )
	{
		worker->init_attr_ret = pthread_attr_init( &(worker->attr) );
		
		if ( worker->init_attr_ret == 0 )
		{
			worker->create_thread_ret = pthread_create
				( &(worker->tid), &(worker->attr), run, worker );
		}
	}
	
	if
	(
		worker->init_attr_ret != 0
		|| worker->open_pipes_ret != 0
		|| worker->create_thread_ret != 0
	)
	{
		_del_worker( worker );
		ret = -1;
	}
	
	return ret;
}

int main_worker( Worker_t run )
{
	int ret = 0, ready, active_workers = 0;
	
	struct memory_group workerv = {0};
	struct memory_block workerb = {0};
	struct worker **workers, *worker;
	
	pipe_t pipes[PIPE_COUNT] = { INVALID_PIPE };
	
	if ( (ret = open_pipes( pipes )) != 0 )
		return EPIPE;
	
	if ( !new_memory_group_total( struct worker *, &workerv, 8 ) )
		return ENOMEM;
		
	workers = workerv.memory_block.block;
	memset( workers, 0, workerv.memory_block.bytes );
	
	worker = new_memory_block( &workerb, sizeof(struct worker) );
	
	if ( !worker )
		goto cleanup;
	
	if ( _new_worker( 1, worker, pipes, run ) != 0 )
		goto cleanup;
	
	workers[++active_workers] = worker;
	
	while ( active_workers > 0 && (ready = poll_pipe( pipes[PIPE_RD] )) >= 0 )
	{
		if ( ready == 1 )
		{
			ssize_t bytes;
			struct worker_msg *worker_msg = NULL, own_msg = {0};
			void *own = &own_msg, *ptr = NULL;
			
			ret = rdpipe( pipes[PIPE_RD], ptr, &bytes );
			
			if ( ret != 0 )
				continue;
			
			worker_msg = ptr;
			
			switch ( worker_msg->type )
			{
			case WORKER_MSG_DIED:
				active_workers--;
				worker = worker_msg->data;
				_del_worker( worker );
				break;
			case WORKER_MSG_ALLOC:
				own_msg = *worker_msg;
				worker_msg->type = WORKER_MSG_WAIT;
				
				for ( int w = 0; w < workerv.total; ++w )
				{
					worker = workers[w];
					if ( worker )
					{
						ret = wrpipe( worker->own_pipes[PIPE_WR], ptr, &bytes );
						
						#pragma message "Best to revisit this later"
						if ( ret != 0 )
						{
							printf
							(
								"Could not recover from "
								"error 0x%08X (%d) '%s', exiting..."
								, ret, ret, strerror(ret)
							);
							exit(EXIT_FAILURE);
						}
						
						while ( (ready = poll_pipe( pipes[PIPE_RD] )) != 1 );
						
						ret = rdpipe( pipes[PIPE_RD], ptr, &bytes );
						
						if ( ret != 0 )
							break;
					}
				}
				
				if ( ret != 0 )
				{
					wrpipe( pipes[PIPE_WR], ptr, &bytes );
					continue;
				}
				
				break;
			}
		}
	}
	
	shut_pipes( pipes );
	
	return ret;
}
