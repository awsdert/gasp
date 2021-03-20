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

#ifdef _WIN32
#	define ENV_HOME "HOMEPATH"
#else
#	define ENV_HOME "HOME"
#endif

struct memory_block get_cfg_path( struct worker *worker )
{
	struct memory_block PATH = {0};
	char *cwd = getcwd(NULL,0), *path = NULL, *home = getenv(ENV_HOME);
	size_t i, size = 50;
	
	size += cwd ? strlen(cwd) : 0;
	size += home ? strlen(home) : 0;
	
	path = lua_memory( worker, path, 0, size );
	
	if ( !path )
		return PATH;
	
	PATH.bytes = size;
	PATH.block = path;
	memset( path, 0, size );
	
	/* Assume we're being run in portable mode 1st */
	if ( cwd )
	{
		if ( access( cwd, W_OK ) == 0 )
		{
			strcat( path, cwd );
			strcat( path, "/../OTG" );
			return PATH;
		}
	}
	
	if ( home )
	{
		strcat( path, home );
		strcat( path, "/.gasp" );
		return PATH;
	}
	
	return PATH;
}

void* worker_lua( struct worker *worker )
{
	int c;
	struct memory_block PATH = get_cfg_path(worker);
	char *path = PATH.block, *tmp = NULL, *init,
		*main_file = "/lua/init.lua", *std_LUA_PATH = getenv("LUA_PATH");
	char *paths[] = { "/lua/?.lua;", "/lua/?;", "/?.lua;", "/?", NULL };
	size_t i, size = PATH.bytes, need = size * 6;
	
	if ( std_LUA_PATH )
		need += strlen(std_LUA_PATH) + 1;
		
	path = lua_memory( worker, path, 0, need );
	
	if ( !path )
	{
		lua_memory( worker, PATH.block, PATH.bytes, 0 );
		goto die;
	}
	
	PATH.block = path;
	PATH.bytes = need;
		
	init = path + size;
	
	memset( init, 0, need - size );
	
	strcat( init, path );
	strcat( init, main_file );
	
	tmp = init + size;
	
	for ( i = 0; paths[i]; ++i )
	{
		strcat( tmp, path );
		strcat( tmp, paths[i] );
	}
	
	if ( std_LUA_PATH )
	{
		strcat( tmp, ";" );
		strcat( tmp, std_LUA_PATH );
	}
	
	setenv( "LUA_PATH", tmp, 1 );
	
	lua_State *L = lua_newstate( lua_memory, worker );
	
	if ( !L )
		goto die;
	
	luaL_openlibs(L);
	
	if ( luaL_dofile(L,init) != 0 )
	{
		dumpstack(L);
	}
	
	lua_close(L);
	
	lua_memory( worker, path, need, 0 );

	die:
	if ( std_LUA_PATH )
		setenv( "LUA_PATH", std_LUA_PATH, 1 );
	return say_worker_died( worker, NULL );
}
