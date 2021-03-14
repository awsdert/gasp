#include <workers.h>

int _new_worker( struct worker *worker, int *pipes, Worker_t run )
{
	int ret;
	
	worker->attr_created = false;
	worker->all_pipes[PIPE_RD] = pipes[PIPE_RD];
	worker->all_pipes[PIPE_WR] = pipes[PIPE_WR];
	
	if ( (ret =open_pipes( worker->own_pipes )) != 0 )
	{
		worker->own_pipes[PIPE_RD] = INVALID_PIPE;
		worker->own_pipes[PIPE_WR] = INVALID_PIPE;
		return ret;
	}
	
	return 0;
}

int _del_worker( struct worker *worker )
{
	if ( worker->own_pipes[PIPE_RD] != INVALID_PIPE )
		shut_pipes( worker->own_pipes );
}

int main_worker( Worker_t run )
{
	int ret = 0, ready, active_workers = 0;
	
	struct memory_group workerv = {0}, *workers;
	struct *worker;
	
	struct pollfd fds = {0};
	
	pipe_t pipes[PIPE_COUNT] =
	{
		worker->mmi_pipes[PIPE_RD],
		worker->mmo_pipes[PIPE_WR]
	};
	
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
			void *ptr = NULL;
			struct worker_msg *worker_msg = NULL;
			struct shared_block *shared_block = NULL;
			struct memory_block *memory_block = NULL;
			
			bytes = rdpipe( pipes[PIPE_RD], (void*)(&ptr) );
			
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
				wrpipe(
					mm_pipes[PIPE_WR], (void*)(&memory_block), sizeof(void*) );
				break;
			case WORKER_MSG_MM_INC:
				
			}
		}
	}
	
	return ret;
}
