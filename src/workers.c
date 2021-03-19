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
	worker->ptr2ptr2src_msg = (void*)(&(worker->ptr2src_msg));
	
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
	void *own = worker->ptr2ptr2own_msg;
	ssize_t bytes;
	
	own_msg->type = msg;
	own_msg->data = obj;
	own_msg->worker = worker;
	own_msg->num = worker->num;
	
	/* While this does create latency it also creates stability */
	return wrpipe( worker->all_pipes[PIPE_WR], own, &bytes );
}
	
int seek_worker_msg( struct worker *worker, int msg )
{
	int ret, sent;
	ssize_t bytes;
	void *ptr = worker->ptr2ptr2src_msg;
	struct worker_msg *own_msg = worker->ptr2own_msg, *src_msg;
	sent = own_msg->type;
	
	worker->ptr2src_msg = NULL;
	
	while ( (ret = poll_pipe( worker->own_pipes[PIPE_RD] )) >= 0 )
	{
		if ( ret == 1 )
		{	
			ret = rdpipe( worker->own_pipes[PIPE_RD], ptr, &bytes );
			
			if ( ret != 0 )
				return ret;
			
			src_msg = worker->ptr2src_msg;
			
			if ( src_msg->type == WORKER_MSG_WAIT )
			{
				send_worker_msg( worker, WORKER_MSG_CONT, own_msg->data );
				continue;
			}
			
			if ( src_msg->type == WORKER_MSG_RESEND )
			{
				send_worker_msg( worker, sent, own_msg->data );
				continue;
			}
			
			if ( msg >= 0 && src_msg->type != msg )
				continue;
			
			break;
		}
	}
	
	return 0;
}

struct worker* say_worker_died( struct worker *worker, worker_panic panic )
{
	int ret;
	
	while ( (ret = send_worker_msg( worker, WORKER_MSG_DIED, worker )) )
	{
		if ( ret != 0 )
			continue;
	}
	
	return worker;
}

struct worker** inc_workers_group( struct memory_group *workers_group );
void del_workers_group( struct memory_group *workers_group );

void worker_died(
	struct memory_group *workers_group, struct worker_msg *worker_msg )
{
	struct worker
		**workers = (struct worker**)get_memory_group_block( workers_group )
		, *worker = worker_msg->data;
		
	worker_msg->type = WORKER_MSG_CONT;
		
	printf("Removing worker %d\n", worker->num );
	
	worker->create_thread_ret = ENOMEDIUM;
	workers[worker->num] = NULL;
	workers_group->count--;
	_del_worker( worker );
	free( worker );
}

struct worker_msg* poll_worker_msg( int *pipes, struct memory_group *workers_group )
{
	int ret;
	
	while
	(
		workers_group->count > 0
		&& (ret = poll_pipe( pipes[PIPE_RD] )) >= 0
	)
	{
		ssize_t bytes;
		struct worker_msg *src_msg = NULL;
		void *_ptr = NULL, *ptr = (void*)&(_ptr);
			
		if ( ret != 1 )
			continue;
			
		ret = rdpipe( pipes[PIPE_RD], ptr, &bytes );
		
		if ( ret != 0 )
		{
			printf( "Error 0x%08X (%d) '%s'\n", ret, ret, strerror(ret) );
			continue;
		}
		
		src_msg = _ptr;
		
		if ( src_msg->type == WORKER_MSG_DIED )
		{
			worker_died( workers_group, src_msg );
			continue;
		}
		
		return src_msg;
	}
	
	return NULL;
}

int main_worker( Worker_t run )
{
	int ret = 0, ready, active_workers = 0;
	
	struct memory_group workers_group = {0}, blocks_group = {0};
	struct memory_block workerb = {0};
	struct shared_block *blocks, *block;
	struct worker **workers, *worker;
	struct worker_msg *src_msg;
	
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
	
	while ( (src_msg = poll_worker_msg(pipes, &workers_group)) )
	{
		ssize_t bytes;
		struct worker_msg own_msg = {0}, tmp_msg = *src_msg;
		void *_own = &own_msg, *own = (void*)(&_own)
			, *_tmp = &tmp_msg, *tmp = (void*)(&_tmp)
			, *_ptr = src_msg, *ptr = (void*)&(_ptr);
		
		if ( src_msg->type == WORKER_MSG_ALLOC )
		{
			struct worker_block *worker_block = src_msg->data;
			struct memory_block *memory_block = worker_block->memory_block;
			
			if ( worker_block->worker )
			{
				tmp_msg.type = WORKER_MSG_WAIT;
				
				for ( int w = 1; w < workers_group.total; ++w )
				{
					worker = workers[w];
					if ( worker )
					{
						ret = wrpipe( worker->own_pipes[PIPE_WR], tmp, &bytes );
						
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
							break;
						}
						
						while
						(
							(src_msg = poll_worker_msg( pipes, &workers_group ))
						)
						{
							if ( src_msg->worker != worker || src_msg->type != WORKER_MSG_CONT )
							{
								own_msg.type = WORKER_MSG_RESEND;
								own_msg.worker = NULL;
								own_msg.data = src_msg;
								ret = wrpipe( src_msg->worker->own_pipes[PIPE_WR], own, &bytes );
								
								if ( ret != 0 )
									goto wrpipe_panic;
							}
							else
								break;
						}
					}
				}
				
				if ( workers_group.count < 1 )
					break;
				
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
			
			tmp_msg.type = WORKER_MSG_CONT;
				
			for ( int w = 1; w < workers_group.total; ++w )
			{
				worker = workers[w];
				
				if ( worker )
				{	
					tmp_msg.worker = worker;
					ret = wrpipe( worker->own_pipes[PIPE_WR], tmp, &bytes );
					
					if ( ret != 0 )
						goto wrpipe_panic;
				}
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
