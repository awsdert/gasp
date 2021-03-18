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
	worker->ptr2src_msg = NULL;
}

void report_error( int err, char const * const msg )
{
	if ( err == 0 )
		return;

	printf( "Error 0x%08X (%d) '%s'", err, err, strerror(err) );
	
	if ( msg )
		printf(", %s", msg );
	
	putc('\n',stdout);
}

struct worker* _new_worker( int num, int *pipes, Worker_t run )
{
	int ret;
	struct worker *worker = malloc( sizeof(struct worker) );
	ret = errno;
	
	if ( !worker )
	{
		report_error( ret, "Could not create worker object" );
		return NULL;
	}
	
	memset( worker, 0, sizeof(struct worker) );
	
	worker->num = num;
	worker->tid = INVALID_TID;
	worker->all_pipes[PIPE_RD] = pipes[PIPE_RD];
	worker->all_pipes[PIPE_WR] = pipes[PIPE_WR];
	worker->open_pipes_ret = open_pipes( worker->own_pipes );
	worker->init_attr_ret = -1;
	worker->create_thread_ret = -1;
	worker->ptr2own_msg = &(worker->own_msg);
	worker->ptr2ptr2own_msg = (void*)(&(worker->ptr2own_msg));
	worker->ptr2ptr2src_msg = &(worker->own_msg);
	
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
		report_error(
			worker->init_attr_ret, "Couldn't initialise worker attribute/s" );
		report_error( worker->open_pipes_ret, "Couldn't open pipes" );
		report_error(
			worker->create_thread_ret, "Couldn't create worker thread" );
		
		_del_worker( worker );
		worker = NULL;
	}
	
	return worker;
}

int send_worker_msg( struct worker *worker, int msg, void *obj )
{
	struct worker_msg *own_msg = worker->ptr2own_msg;
	void *own = worker->ptr2ptr2own_msg, *_ptr = NULL, *ptr = (void*)&(_ptr);
	ssize_t bytes;
	
	own_msg->type = WORKER_MSG_ALLOC;
	own_msg->data = ptr;
	
	/* While this does create latency it also creates stability */
	return wrpipe( worker->all_pipes[PIPE_WR], own, &bytes );
}
	
int seek_worker_msg( struct worker *worker, int msg )
{
	int ret;
	ssize_t bytes;
	void *ptr = worker->ptr2ptr2src_msg;
	
	worker->ptr2src_msg = NULL;
	
	printf("Worker %d is seeking a message\n", worker->num );
	
	while ( (ret = poll_pipe( worker->own_pipes[PIPE_RD] )) >= 0 )
	{
		printf("Worker %d is checking if pipe is ready\n", worker->num );
		
		if ( ret == 1 )
		{
			struct worker_msg *own_msg = worker->ptr2own_msg, *src_msg;
			
			printf("Worker %d is reading a message pointer\n", worker->num );
			
			ret = rdpipe( worker->own_pipes[PIPE_RD], ptr, &bytes );
			
			if ( ret != 0 )
				return ret;
			
			src_msg = worker->ptr2src_msg;
			
			printf("Worker %d is checking for message type\n", worker->num );
			
			if ( src_msg->type == WORKER_MSG_WAIT )
			{
				send_worker_msg( worker, WORKER_MSG_CONT, own_msg->data );
				continue;
			}
			
			if ( msg >= 0 && src_msg->type != msg )
				continue;
			
			break;
		}
	}
	
	printf("Worker %d successfully sought a message\n", worker->num );
	
	return 0;
}

struct worker* say_worker_died( struct worker *worker, worker_panic panic )
{
	int ret = send_worker_msg( worker, WORKER_MSG_DIED, worker );
	
	if ( ret != 0 )
	{
		if ( panic ) panic(worker);
		exit(EXIT_FAILURE);
	}
	
	return worker;
}

struct worker** inc_workers_group( struct memory_group *workers_group );
void del_workers_group( struct memory_group *workers_group );

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
	
	workers_group.Tsize = sizeof(struct worker*);
	blocks_group.Tsize = sizeof(struct shared_block);
	
	workers = inc_workers_group( &workers_group );
	
	if ( !workers )
	{
		ret = ENOMEM;
		goto cleanup;
	}
	
	memset( workers, 0, workers_group.memory_block.bytes );
	
	worker = _new_worker( 1, pipes, run );
	
	if ( !worker )
	{
		puts("Failed to create worker object, exiting...");
		goto cleanup;
	}
	
	workers_group.count++;
	workers[workers_group.count] = worker;
	
	printf( "Worker pointer 1 is now %p\n", workers[1] );
	
	puts("Starting main worker loop");
	
	while ( workers_group.count > 0 && (ready = poll_pipe( pipes[PIPE_RD] )) >= 0 )
	{
		ssize_t bytes;
		struct worker_msg *worker_msg = NULL, own_msg = {0};
		void *_own = &own_msg, *own = (void*)(&_own)
			, *_ptr = NULL, *ptr = (void*)&(_ptr);
			
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
			workers_group.count--;
			worker = worker_msg->data;
			worker->create_thread_ret = ENOMEDIUM;
			workers[worker->num] = NULL;
			_del_worker( worker );
			free( worker );
			break;
		case WORKER_MSG_ALLOC:
		{
			struct worker_block *worker_block = worker_msg->data;
			struct memory_block *memory_block = worker_block->memory_block;
			
			if ( worker_block->worker )
			{
				worker_msg->type = WORKER_MSG_WAIT;
				
				for ( int w = 1; w < workers_group.total; ++w )
				{
					worker = workers[w];
					if ( worker )
					{
						puts("Main worker is sending wait message");
						ret = wrpipe( worker->own_pipes[PIPE_WR], ptr, &bytes );
						
						#pragma message "Best to revisit this later"
						if ( ret != 0 )
						{
							wrpipe_panic:
							printf
							(
								"Could not recover from "
								"error 0x%08X (%d) '%s', exiting..."
								, ret, ret, strerror(ret)
							);
							exit(EXIT_FAILURE);
						}
						
						printf("Main worker is waiting on a response from "
								"worker %d", worker->num );
						
						while ( (ready = poll_pipe( pipes[PIPE_RD] )) >= 0 )
						{
							puts("Main worker is checking if pipe is ready");
							
							if ( ready == 1 )
							{
								ret = rdpipe( pipes[PIPE_RD], ptr, &bytes );
								
								if ( ret != 0 )
									break;
								
								if ( worker_msg->type != WORKER_MSG_CONT || worker_msg->worker != worker )
								{
									puts("Main worker is sending resend message");
									
									own_msg.type = WORKER_MSG_RESEND;
									own_msg.worker = NULL;
									own_msg.data = worker_msg;
									ret = wrpipe( worker_msg->worker->own_pipes[PIPE_WR], own, &bytes );
									
									if ( ret != 0 )
										goto wrpipe_panic;
								}
								else
									break;
							}
						}
					}
				}
				
				puts("Main worker is performing allocation");
				
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
						temp = (struct shared_block*)
							inc_memory_group_total( &blocks_group, 64 );
						
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
			
			for ( int w = 1; w < workers_group.total; ++w )
			{
				worker = workers[w];
				
				if ( worker )
				{
					printf("Main worker is sending continue message to "
						"worker %d", worker->num );
					
					ret = wrpipe( worker->own_pipes[PIPE_WR], ptr, &bytes );
					
					if ( ret != 0 )
						goto wrpipe_panic;
				}
			}
			
			break;
		}
		}
	}
	
	puts("Ending main worker");
	
	cleanup:
	
	del_workers_group( &workers_group );
	
	shut_pipes( pipes );
	
	return ret;
}

struct worker** inc_workers_group( struct memory_group *workers_group )
{
	return (struct worker**)inc_memory_group_total( workers_group, 8 );
}

void del_workers_group( struct memory_group *workers_group )
{
	struct worker *worker, **workers =
		(struct worker **)get_memory_group_block( workers_group );
	
	for ( int w = 0; w < workers_group->total; ++w )
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
