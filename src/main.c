#include <workers.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

void* memory( void* ud, void* ptr, size_t osize, size_t nsize );
void* worker_lua( struct worker *worker );

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

/* Robbed from
 * https://stackoverflow.com/questions/59091462/from-c-how-can-i-print-the-contents-of-the-lua-stack
 */

void dumpstack (lua_State *L)
{
	int top=lua_gettop(L);
	for (int i=1; i <= top; i++)
	{
		printf("%d\t%s\t", i, luaL_typename(L,i));
		switch (lua_type(L, i))
		{
			case LUA_TNUMBER:
				printf("%g\n",lua_tonumber(L,i));
				break;
			case LUA_TSTRING:
				printf("%s\n",lua_tostring(L,i));
				break;
			case LUA_TBOOLEAN:
				printf("%s\n", (lua_toboolean(L, i) ? "true" : "false"));
				break;
			case LUA_TNIL:
				printf("%s\n", "nil");
				break;
			default:
				printf("%p\n",lua_topointer(L,i));
				break;
		}
	}
}

void* worker_lua( struct worker *worker )
{
	int c;
	char *cwd = getcwd(NULL,0), *path = NULL, *tmp = NULL, *main_file = "test.lua";
	size_t i, size = strlen(cwd) + strlen(main_file) + 3;
	path = lua_memory( worker, path, size, size + BUFSIZ );
	
	if ( !path )
		goto die;
	
	memset( path, 0, size );
	
	printf( "path is '%s'\n", path );
	
	strcat( path, cwd );
	
	printf( "path is '%s'\n", path );
	
	strcat( path, "/" );
	
	printf( "path is '%s'\n", path );
	
	strcat( path, main_file );
	
	printf( "path is '%s'\n", path );
	
	lua_State *L = lua_newstate( lua_memory, worker );
	
	if ( !L )
		goto die;
	
	luaL_openlibs(L);
	
	if ( luaL_dofile(L,path) != 0 )
	{
		dumpstack(L);
	}
	
	lua_close(L);
	
	lua_memory( worker, path, size, 0 );

	die:
	return say_worker_died( worker, NULL );
}
