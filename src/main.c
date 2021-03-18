#include <workers.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

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
		struct worker *worker = ud;
		struct worker_msg *own_msg = worker->ptr2own_msg, *src_msg;
		struct worker_block worker_block = {0};
		struct memory_block memory_block = {0};
		
		memory_block.block = ptr;
		memory_block.bytes = osize;
		worker_block.want = nsize;
		worker_block.memory_block = &memory_block;
		
		/* While this does create latency it also creates stability */
		ret = send_worker_msg( worker, WORKER_MSG_ALLOC, &worker_block );
		
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
		
		ret = seek_worker_msg( worker, WORKER_MSG_CONT );
		
		return (memory_block.bytes >= nsize) ? memory_block.block : NULL;
	}
	
	return NULL;
}

void* worker_lua( struct worker *worker )
{	
	puts("Starting lua worker");
	
	lua_worker = worker;
	
	lua_State *L = lua_newstate( lua_memory, worker );
	
	if ( !L )
		goto die;
		
	puts("Created lua state");
	
	luaL_openlibs(L);
	
	puts("Opened standard lua libs");
	
	luaL_dofile(L, "test.lua");
	
	lua_close(L);

	die:
	puts("Ending lua worker");
	return say_worker_died( worker, NULL );
}
