#include "gasp.h"
void error_cb( char *from, int code, char const *desc ) {
	fprintf( stderr, "%s: Error %08X\n"
		"External Msg: '%s'\n"
		"Internal Msg: '%s'\n",
		from, code, strerror(code), desc );
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
	kvpair_t *args = NULL;
	char *HOME = NULL, *path = NULL, *PWD = NULL, gasp[] = "gasp",
		*LUA_PATH = NULL, *LUA_CPATH = NULL;
	size_t leng = BUFSIZ
#if 0
	, size = 0
#endif
	;
	intptr_t addr = 0;
#if 0
	lua_CFunction old_lua_panic_cb;
#endif
	lua_State *L = NULL;
	if ( (ret = arguments( argc, argv, &ARGS, &leng )) != EXIT_SUCCESS ) {
		ERRMSG( ret, "Couldn't get argument pairs" );
		goto cleanup;
	}
	leng += BUFSIZ;
	args = ARGS.space.block;
	if ( !(HOME = getenv("HOME")) || !(PWD = getenv("PWD")) ) {
		ret = errno;
		ERRMSG( ret, "Couldn't get $(HOME) and/or $(PWD)" );
		goto cleanup;
	}
	leng = strlen(PWD) + 20;
	LUA_PATH = calloc( leng, 1 );
	LUA_CPATH = calloc( leng, 1 );
	sprintf(LUA_PATH,"%s/lua/?.lua",PWD);
	sprintf(LUA_CPATH,"%s/?.so",PWD);
	setenv("LUA_PATH",LUA_PATH,0);
	setenv("LUA_CPATH",LUA_CPATH,0);
	if ( !(L = luaL_newstate()) ) {
		ret = errno;
		ERRMSG( ret, "Couldn't create lua instance" );
		goto cleanup;
	}
#if 0
	old_lua_panic_cb =
#endif
	lua_atpanic(L,lua_panic_cb);
	luaL_openlibs(L);
	/* Just a hack for slipups upstream */
	(void)luaL_dostring(L,"loadlib = package.loadlib");
	lua_proc_create_class(L);
	lua_pushcfunction(L,lua_proc_locate_name);
	lua_setglobal(L,"proc_locate_name");
#if 1
	path = calloc( leng, 1 );
	sprintf( path, "%s/lua/gasp.lua", PWD );
	if ( luaL_dofile(L, path ) )
		printf("Failed:\n%s\n", lua_tostring(L,-1));
	free(path);
	path = NULL;
#endif
	lua_close(L);
	free(LUA_CPATH);
	free(LUA_PATH);
#if 0
	if ( (noticed = proc_locate_name( &ret, gasp, &nodes, 0 )) ) {
		fputs( "Found:\n", stderr);
		path = calloc( size, 1 );
		if ( path ) {
			sprintf( path, "%s/.gasp", HOME );
			if ( access(path,0) != 0 && mkdir(path,0755) != 0 ) {
				ret = errno;
				ERRMSG( ret, "access() or mkdir() failed" );
				fprintf( stderr, "Path: '%s'\n", path );
			}
			sprintf( path, "%s/.gasp/test.aobscan", HOME );
			if ( (into = open(path, O_CREAT | O_RDWR, 0755 )) < 0 ) {
				ret = errno;
				ERRMSG( ret, "open() failed" );
				fprintf( stderr, "Path: '%s'\n", path );
			}
		}
		for ( i = 0; i < nodes.count; ++i ) {
			(void)fprintf( stderr, "%04X '%s' launched with\n%s\n",
				noticed[i].entryId,
				(char*)(noticed[i].name.block),
				(char*)(noticed[i].cmdl.block) );
			if ( (handle =
				proc_handle_open( &ret, noticed[i].entryId )) ) {
				if ( ret != EXIT_SUCCESS )
					ERRMSG( ret, "proc_handle_open() failed" );
				if ( !(count = proc_aobscan( &ret, into, handle,
					(uchar*)gasp, strlen(gasp), 0, INTPTR_MAX, 1 )) ) {
					if ( ret != EXIT_SUCCESS )
						ERRMSG( ret, "proc_aobscan() failed" );
					continue;
				}
				
				gasp_lseek( into, 0, SEEK_SET );
				for ( a = 0; a < count; ++a ) {
					(void)read( into, &addr, sizeof(void*) );
					fprintf( stderr, "Got address %p\n", (void*)addr );
					if ( proc_change_data( &ret, handle, addr,
						"help", 4 )  != strlen(gasp) ) {
						ret = errno;
						ERRMSG( ret, "Couldn't write to memory" );
					}
					//fprintf(stderr, "gasp = '%s'\n", gasp);
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
	else if ( ret == EXIT_SUCCESS ) {
		ret = ENOENT;
		ERRMSG( ret, "Couldn't find process" );
	}
	else
		ERRMSG( ret, "Couldn't find process" );
#else
	(void)into;
	(void)i;
	(void)a;
	(void)count;
	(void)noticed;
	(void)handle;
	(void)gasp;
	(void)addr;
#endif
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
	(void)less_nodes( kvpair_t, NULL, &ARGS, 0 );
	(void)less_nodes( proc_notice_t, NULL, &nodes, 0 );
	return ret;
}
