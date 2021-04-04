#include "gasp.h"

void* worker_lua( struct worker *worker );

int main()
{
	//printf("Gasp Alpha\n");
	
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

#ifdef _WIN32
#define LIB_SFX ".dll"
#else
#define LIB_SFX ".so"
#endif

struct env_pair { char * key; char *val; char *ret; };

void set_env_pair( struct env_pair *env_pairs, char *key, char *val, char *ret )
{
	for ( int i = 0; env_pairs[i].key; ++i )
	{
		if ( env_pairs[i].key == key )
		{
			if ( val )
			{
				env_pairs[i].ret = ret;
				setenv( key, val, !!ret );
				if ( getenv(key) == val )
					env_pairs[i].val = val;
			}
			else if ( getenv( key ) == env_pairs[i].val )
				setenv( key, env_pairs[i].ret, 1 );
		}
	}
}

void* worker_lua( struct worker *worker )
{
	int c;
	struct memory_block PATH = get_cfg_path(worker);
	char *path = PATH.block, *lua_path = NULL, *lua_cpath, *init,
		*main_file = "/init.lua"
		, *std_LUA_PATH = getenv("LUA_PATH")
		, *std_LUA_CPATH = getenv("LUA_CPATH");
	char *paths[] =
	{
		"/lib/?.lua;"
		, "/?.lua"
		, NULL
	};
	char *cpaths[] =
	{
		"/lib/?" LIB_SFX ";"
		, "/?" LIB_SFX
		, NULL
	};
	struct env_pair env_pairs[] =
	{
		{ "LUA_PATH", NULL, std_LUA_PATH },
		{ "LUA_CPATH", NULL, std_LUA_CPATH },
		{ "GASP_PATH", NULL, NULL },
		{ "GASP_LIB_SFX", LIB_SFX, NULL },
		{ NULL, NULL, NULL }
	};
	
	size_t i, size = PATH.bytes, need = size * 2
		, lua_path_used = std_LUA_PATH ? strlen(std_LUA_PATH) + 1: 1
		, lua_cpath_used = std_LUA_CPATH ? strlen(std_LUA_CPATH) + 1: 1;
	
	for ( i = 0; paths[i]; ++i, lua_path_used += size );
	for ( i = 0; cpaths[i]; ++i, lua_cpath_used += size );
	
	need += lua_path_used + lua_cpath_used;
		
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
	
	lua_path = init + size;
	lua_cpath = lua_path + lua_path_used;
	
	for ( i = 0; paths[i]; ++i )
	{
		strcat( lua_path, path );
		strcat( lua_path, paths[i] );
	}
	
	if ( std_LUA_PATH )
	{
		strcat( lua_path, ";" );
		strcat( lua_path, std_LUA_PATH );
	}
	
	for ( i = 0; cpaths[i]; ++i )
	{
		strcat( lua_cpath, path );
		strcat( lua_cpath, cpaths[i] );
	}
	
	if ( std_LUA_CPATH )
	{
		strcat( lua_cpath, ";" );
		strcat( lua_cpath, std_LUA_CPATH );
	}
	
	set_env_pair( env_pairs, "LUA_PATH", lua_path, std_LUA_PATH );
	set_env_pair( env_pairs, "LUA_CPATH", lua_cpath, std_LUA_CPATH );
	set_env_pair( env_pairs, "GASP_PATH", path, 0 );
	set_env_pair( env_pairs, "GASP_LIB_SFX", LIB_SFX, 0 );
	
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
	
	lua_memory( worker, path, need, 0 );

	die:
	set_env_pair( env_pairs, "LUA_PATH", NULL, NULL );
	set_env_pair( env_pairs, "LUA_CPATH", NULL, NULL );
	set_env_pair( env_pairs, "GASP_PATH", NULL, NULL );
	set_env_pair( env_pairs, "GASP_LIB_SFX", NULL, NULL );
	return say_worker_died( worker, NULL );
}
