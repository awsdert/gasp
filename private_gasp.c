#include "gasp.h"
void error_cb( char *from, int code, char const *desc ) {
	ERRMSG( code, desc );
}

int lua_panic_cb( lua_State *L ) {
	puts("Error: Lua traceback...");
	luaL_traceback(L,L,"[error]",-1);
	puts(lua_tostring(L,-1));
	return 0;
}

void *lua_allocator( void *ud, void *ptr, size_t size, size_t want ) {
	void *tmp = NULL;
	(void)ud;
	if ( ptr ) {
		if ( !want ) {
			free(ptr);
			return NULL;
		}
		tmp = realloc(ptr,want);
		if ( !tmp )
			return (want <= size) ? ptr : NULL;
		return tmp;
	}
	return malloc( want );
}

int main( int argc, char *argv[] ) {
	int ret = EXIT_SUCCESS, into;
	int arg;
	node_t i, a, count;
	nodes_t nodes = {0}, ARGS = {0};
	proc_notice_t *noticed;
	proc_handle_t *handle;
	key_val_t *args = NULL;
	char *HOME = NULL, *path = NULL, *PWD = NULL, *gasp = "gasp",
		*DISPLAY = NULL, *LUA_PATH = NULL, *LUA_CPATH = NULL;
	size_t size = 0, leng = BUFSIZ;
	intptr_t addr = 0;
	lua_CFunction old_lua_panic_cb;
	if ( (ret = arguments( argc, argv, &ARGS, &leng )) != EXIT_SUCCESS ) {
		ERRMSG( ret, "Couldn't get argument pairs" );
		goto cleanup;
	}
	leng += BUFSIZ;
	args = ARGS.space.block;
	DISPLAY = getenv("DISPLAY");
	if ( !(HOME = getenv("HOME")) || !(PWD = getenv("PWD")) ) {
		ret = EXIT_FAILURE;
		ERRMSG( errno, "Couldn't get $(HOME) and/or $(PWD)" );
		goto cleanup;
	}
	size = strlen(HOME) + 32;
	leng += strlen(PWD) + 4;
	leng += strlen(HOME) + 4;
	leng += strlen(DISPLAY) + 4;
	leng = strlen(PWD) + 7;
	LUA_PATH = calloc( leng, 1 );
	LUA_CPATH = calloc( leng, 1 );
	sprintf(LUA_PATH,"%s/?.lua",PWD);
	sprintf(LUA_CPATH,"%s/?.so",PWD);
	setenv("LUA_PATH",LUA_PATH,0);
	setenv("LUA_CPATH",LUA_CPATH,0);
	lua_State *L = NULL;
	if ( !(L = luaL_newstate()) ) {
		ERRMSG( errno, "Couldn't create lua instance" );
		ret = EXIT_FAILURE;
		goto cleanup;
	}
	old_lua_panic_cb = lua_atpanic(L,lua_panic_cb);
	luaL_openlibs(L);
	/* Just a hack for slipups upstream */
	luaL_dostring(L,"loadlib = package.loadlib");
#if 0
	leng += 3;
	path = calloc( leng, 1 );
	sprintf(path, "%s/gasp.lua", PWD );
	if ( luaL_dofile(L, path) )
		printf("Failed:\n%s\n", lua_tostring(L,-1));
	free(path);
	path = NULL;
#endif
	lua_close(L);
	free(LUA_CPATH);
	free(LUA_PATH);
	printf( "Searching for processes containing '%s'\n", gasp );
	if ( (noticed = proc_locate_name( &ret, gasp, &nodes, 0 )) ) {
		puts("Found:");
		path = calloc( size, 1 );
		if ( path ) {
			sprintf( path, "%s/.gasp", HOME );
			if ( access(path,0) != 0 && mkdir(path,0755) != 0 ) {
				ret = errno;
				ERRMSG( ret, "access() or mkdir() failed" );
				fputs( path, stderr );
			}
			sprintf( path, "%s/.gasp/test.aobscan", HOME );
			if ( (into = open(path, O_CREAT | O_RDWR, 0755 )) < 0 ) {
				ret = errno;
				ERRMSG( ret, "open() failed" );
				puts( path );
			}
		}
		for ( i = 0; i < nodes.count; ++i ) {
			(void)printf( "%04X '%s'\n",
				noticed[i].entryId, (char*)(noticed[i].name.block) );
			if ( (handle =
				proc_handle_open( &ret, noticed[i].entryId )) ) {
				if ( ret != EXIT_SUCCESS )
					ERRMSG( ret, "proc_handle_open() failed" );
				printf("rdMemFd = %d, wrMemFd = %d\n",
					handle->rdMemFd, handle->wrMemFd );
				if ( !(count = proc_aobscan( &ret, into, handle,
					(uchar*)gasp, strlen(gasp), 0, INTPTR_MAX )) &&
					ret != EXIT_SUCCESS )
					ERRMSG( ret, "proc_aobscan() failed" );
				gasp_lseek( into, 0, SEEK_SET );
				for ( a = 0; a < count; ++a ) {
					(void)read( into, &addr, sizeof(void*) );
					printf( "Got address %p, ", (void*)addr );
					if ( proc_change_data( &ret, handle, addr,
						"help.elf", strlen(gasp) )  != strlen(gasp) ) {
						ret = errno;
						ERRMSG( ret, "Couldn't write to memory" );
					}
					printf("gasp = %s\n", gasp);
				}
				proc_handle_shut( handle );
			}
			less_space( NULL, &(noticed[i].name), 0 );
		}
		close(into);
		if ( path ) {
			remove( path );
			free( path );
			path = NULL;
		}
		ret = EXIT_SUCCESS;
	}
	else if ( ret == EXIT_SUCCESS )
		ret = EXIT_FAILURE;
	cleanup:
	if ( path ) free( path );
	if ( ret != EXIT_SUCCESS ) {
		ERRMSG( ret, "Test failed" );
		ret = EXIT_FAILURE;
	}
	if ( args ) {
		for ( arg = 1; arg < argc; ++arg ) {
			less_space( NULL, &(args[arg].key), 0 );
			less_space( NULL, &(args[arg].val), 0 );
		}
	}
	(void)less_nodes( key_val_t, NULL, &ARGS, 0 );
	(void)less_nodes( proc_notice_t, NULL, &nodes, 0 );
	return ret;
}
