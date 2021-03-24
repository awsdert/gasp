#include "gasp.h"

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
	char *paths[] = {
#ifdef _WIN32
		"/lib/?.dll;"
#else
		"/lib/?.so;"
#endif
		, "/lib/?;", "/lua/?.lua;", "/lua/?;", "/?.lua;", "/?", NULL
	};
	size_t i, size = PATH.bytes, need = size * 8;
	
	if ( std_LUA_PATH )
		need += strlen(std_LUA_PATH) + 1;
		
	path = lua_memory( worker, path, 0, need );
	
	if ( !path )
	{
		lua_memory( worker, PATH.block, PATH.bytes, 0 );
		goto die;
	}
	
	setenv( "GASP_PATH", path, 0 );
	
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
	
	puts(tmp);
	
	if ( std_LUA_PATH )
	{
		strcat( tmp, ";" );
		strcat( tmp, std_LUA_PATH );
	}
	
	setenv( "LUA_PATH", tmp, 1 );
	
	lua_State *L = lua_newstate( lua_memory, worker );
	
	if ( !L )
		goto die;
	
	lua_atpanic( L, lua_panic_cb );
	
	luaL_openlibs(L);
	
	push_global_cfunc( L, "dump_lua_stack", lua_dumpstack );
	
	lua_create_gasp(L);
	
	if ( luaL_dofile(L,init) != 0 )
	{
		lua_dumpstack(L);
	}
	
	lua_close(L);
	
	if ( getenv( "GASP_PATH" ) == path )
		setenv( "GASP_PATH", NULL, 1 );
	
	lua_memory( worker, path, need, 0 );

	die:
	if ( std_LUA_PATH )
		setenv( "LUA_PATH", std_LUA_PATH, 1 );
	return say_worker_died( worker, NULL );
}
