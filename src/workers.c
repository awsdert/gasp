#include <workers.h>

int _del_worker( struct worker *worker )
{	
	if ( worker->open_pipes_ret == 0 )
	{
		if ( worker->init_attr_ret == 0 )
		{
			if ( worker->create_thread_ret == 0 )
			{
				struct worker_msg worker_msg = {0};
				void *ptr = &worker_msg;
				
				worker_msg.type = WORKER_MSG_TERM;
				worker_msg.data = worker;
				
				wrpipe( worker->own_pipes[PIPE_WR], (void*)(&ptr) );
				
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
	
	struct memory_group workerv = {0}, *workers;
	struct worker *worker;
	
	struct pollfd fds = {0};
	
	pipe_t pipes[PIPE_COUNT] = { INVALID_PIPE };
	
	if ( (ret = open_pipes( pipes )) != 0 )
		return EPIPE;
	
	if ( !new_memory_group_total( struct memory_block, &workerv, 8 ) )
		return ENOMEM;
		
	workers = get_memory_group_group( struct memory_block, &workerv );
	
	worker = new_memory_block( workers + 1, sizeof(struct worker) );
	
	if ( !worker )
		goto cleanup;
	
	if ( _new_worker( worker, pipes, run ) != 0 )
		goto cleanup;
		
	active_workers++;
	
	while ( active_workers > 0 && (ready = poll_pipe( pipes[PIPE_RD] )) >= 0 )
	{
		if ( ready == 1 )
		{
			ssize_t bytes;
			struct worker_msg *worker_msg = NULL, own_msg = {0};
			struct shared_block *shared_block = NULL;
			struct memory_block *memory_block = NULL;
			void *own = &worker_msg, *ptr = NULL;
			
			bytes = rdpipe( pipes[PIPE_RD], (void*)(&ptr) );
			
			if ( bytes < 0 )
				continue;
			
			worker_msg = ptr;
			
			if
			(
				worker_msg->type >= WORKER_MSG_MEM_NEW &&
				worker_msg->type <= WORKER_MSG_MEM_DEL
			)
			{
				shared_block = worker_msg->data;
				memory_block = shared_block->memory_block;
			}
			
			switch ( worker_msg->type )
			{
			case WORKER_DIED:
				active_workers--;
				worker = worker_msg->data;
				_del_worker( worker );
				break;
			case WORKER_MSG_MM_NEW:
				(void)new_memory_block( memory_block, shared_block->want );
				own_msg.type = WORKER_MSG_CONT;
				own_msg.data = worker;
				wrpipe(
					pipes[PIPE_WR], (void*)(&memory_block), sizeof(void*) );
				break;
			case WORKER_MSG_MM_INC:
				(void)new_memory_block( memory_block, shared_block->want );
				wrpipe(
					pipes[PIPE_WR], (void*)(&memory_block), sizeof(void*) );
				break;
			}
		}
	}
	
	shut_pipes( pipes );
	
	return ret;
}
