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
				
				pthread_cancel( worker->tid );
				
				pthread_join( worker->tid, &ptr );
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
	worker->ptr2own_msg = &(worker->own_msg);
	worker->ptr2ptr2own_msg = (void*)(&(worker->ptr2own_msg));
	
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

struct worker* say_worker_died( struct worker *worker )
{
	int ret;
	ssize_t bytes;
	struct worker_msg *own_msg = worker->ptr2own_msg;
	
	/* Only way to identify the worker after it died */
	own_msg->type = WORKER_MSG_DIED;
	own_msg->data = worker;
	
	printf( "Reporting Death of Worker %d\n", worker->num );
	
	ret = wrpipe( worker->all_pipes[PIPE_WR], worker->ptr2ptr2own_msg, &bytes );
	
	return worker;
}

int main_worker( Worker_t run )
{
	int ret = 0, ready, active_workers = 0;
	
	struct memory_group workers_group = {0}, blocks_group = {0};
	struct memory_block workerb = {0};
	struct shared_block *blocks, *block;
	struct worker **workers, *worker;
	
	pipe_t pipes[PIPE_COUNT] = { INVALID_PIPE };
	
	if ( (ret = open_pipes( pipes )) != 0 )
		return EPIPE;
	
	if ( !new_memory_group_total( struct worker *, &workers_group, 8 ) )
	{
		ret = ENOMEM;
		goto cleanup;
	}
		
	workers = workers_group.memory_block.block;
	memset( workers, 0, workers_group.memory_block.bytes );
	
	worker = new_memory_block( &workerb, sizeof(struct worker) );
	
	if ( !worker )
	{
		puts("Failed to create worker object, exiting...");
		goto cleanup;
	}
	
	memset( worker, 0, sizeof(struct worker) );
	
	if ( _new_worker( 1, worker, pipes, run ) != 0 )
	{
		puts("Failed to create worker thread, exiting..." );
		goto cleanup;
	}
	
	workers[++active_workers] = worker;
	
	puts("Starting main worker loop");
	
	while ( active_workers > 0 && (ready = poll_pipe( pipes[PIPE_RD] )) >= 0 )
	{
		ssize_t bytes;
		struct worker_msg *worker_msg = NULL, own_msg = {0};
		void *own = &own_msg, *_ptr = NULL, *ptr = (void*)&(_ptr);
			
		if ( ready != 1 )
			continue;
			
		ret = rdpipe( pipes[PIPE_RD], ptr, &bytes );
		
		if ( ret != 0 )
		{
			printf( "Error 0x%08X (%d) '%s'\n", ret, ret, strerror(ret) );
			continue;
		}
		
		puts("Main worker is reading from pointer");
		
		worker_msg = _ptr;
		
		switch ( worker_msg->type )
		{
		case WORKER_MSG_DIED:
			puts("Main worker is removing a worker");
			active_workers--;
			worker = worker_msg->data;
			worker->create_thread_ret = ENOMEDIUM;
			_del_worker( worker );
			break;
		case WORKER_MSG_ALLOC:
		{
			struct worker_block *worker_block = worker_msg->data;
			struct memory_block *memory_block = worker_block->memory_block;
			
			if ( worker_block->worker )
			{
				own_msg = *worker_msg;
				worker_msg->type = WORKER_MSG_WAIT;
				
				for ( int w = 0; w < workers_group.total; ++w )
				{
					worker = workers[w];
					if ( worker )
					{
						ret = wrpipe
						(
							worker->own_pipes[PIPE_WR]
							, ptr
							, &bytes
						);
						
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
				
				if ( worker_block->want )
				{
					int i = 0;
					
					for ( ; i < blocks_group.total; ++i )
					{
						block = &blocks[i];
						if ( block->block == memory_block->block )
						{
							block->holders++;
							break;
						}
					}
					
					if ( i == blocks_group.total )
					{
						for ( i = 0; i < blocks_group.total; ++i )
						{
							block = &blocks[i];
							if ( !(block->block) )
							{
								block->holders++;
								block->block = memory_block->block;
								blocks_group.count++;
								break;
							}
						}
					}
					
					if ( i == blocks_group.total )
					{
						void *temp;
						temp = inc_memory_group_total
							( struct shared_block, &blocks_group, 64 );
						
						if ( temp )
						{
							blocks = temp;
							block = &blocks[i];
							block->holders++;
						}
					}
					
					if ( i < blocks_group.total )
					{
						inc_memory_block( memory_block, worker_block->want );
						block->block = memory_block->block;
					}
				}
				else
				{
					for ( int i = 0; i < blocks_group.total; ++i )
					{
						block = &blocks[i];
						if ( block->block == memory_block->block )
						{
							block->holders--;
							if ( !block->holders )
							{
								del_memory_block( memory_block );
								block->block = NULL;
								blocks_group.count--;
								break;
							}
						}
					}
				}
			}
			else if ( worker_block->want )
				alt_memory_block( memory_block, worker_block->want );
			else
				del_memory_block( memory_block );
			
			worker_msg->type = WORKER_MSG_CONT;
			for ( int w = 0; w < workers_group.total; ++w )
			{
				worker = workers[w];
				if ( worker )
				{
					ret = wrpipe
					(
						worker->own_pipes[PIPE_WR]
						, ptr
						, &bytes
					);
					
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
				}
			}
			
			break;
		}
		}
	}
	
	puts("Ending main worker");
	
	cleanup:
	
	if ( workers )
	{
		for ( int w = 0; w < workers_group.total; ++w )
		{
			worker = workers[w];
			if ( worker )
			{
				_del_worker( worker );
				free( worker );
			}
			workers[w] = NULL;
		}
	}
	
	shut_pipes( pipes );
	
	return ret;
}
