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

int set_scope( char const *path, int set ) {
	FILE *file;
	int get = -1;
	if ( set < 0 ) set = 0;
	if ( access( path, F_OK ) != 0 ) {
		get = 0;
		if ( (file = fopen( path, "w" )) ) {
			fprintf( file, "%d", set );
			fflush( file );
			fclose( file );
			return 0;
		}
		goto fail_open;
	}
	if ( !(file = fopen(path,"rw")) ) {
		fail_open:
		if ( set < 1 ) {
			fprintf( stderr, "%s:1:0: Could not set %d"
				", might not be able to hook apps\n", path, set );
		}
		else {
			fprintf( stderr, "%s:1:0: Could not restore %d"
				", need to manually restore!\n", path, set );
		}
		return 0;
	}
	fscanf( file, "%d", &get );
	if ( set < get ) {
		fprintf( stderr, "%s:1:0: ptrace scope is restrained by"
		" value %d,\nAttempting to override to %d, "
		"Do this manually if gasp fails\n", path, get, set );
		fseek( file, 0, SEEK_SET );
		fprintf( file, "%d", set );
		fflush( file );
	}
	else if ( set > get ) {
		fprintf( stderr, "%s:1:0: Attempting to restore %d"
				", manually restore if fail!\n", path, set );
		fseek( file, 0, SEEK_SET );
		fprintf( file, "%d", set );
		fflush( file );
	}
	fclose( file );
	file = NULL;
	return get;
}

int main( int argc, char *argv[] ) {
	int ret = EXIT_SUCCESS, into;
	int arg;
	node_t i, a, count;
	nodes_t nodes = {0}, ARGS = {0};
	proc_notice_t *noticed;
	proc_handle_t *handle;
	kvpair_t *args = NULL;
	int scope, conf;
	char const *ptrace_scope = "/proc/sys/kernel/yama/ptrace_scope",
		*ptrace_conf = "/etc/sysctl.d/10-ptrace.conf";
	char *HOME = NULL, *path = NULL, *PWD = NULL, gasp[] = "gasp",
		*LUA_PATH = NULL, *LUA_CPATH = NULL, *GASP_PATH = NULL;
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
	
	scope = set_scope( ptrace_scope, 0 );
	conf = set_scope( ptrace_conf, 0 );
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
	leng = strlen(HOME) + 100;
	GASP_PATH = calloc( leng, 1 );
	sprintf( GASP_PATH, "%s/gasp", HOME );
	leng += strlen(PWD) + 20;
	LUA_PATH = calloc( leng, 1 );
	LUA_CPATH = calloc( leng, 1 );
	sprintf(LUA_PATH,"%s/lua/?.lua;%s/?.lua",PWD,GASP_PATH);
	sprintf(LUA_CPATH,"%s/?.so",PWD);
	setenv("GASP_PATH",GASP_PATH,0);
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
	lua_create_gasp(L);
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
#if 0
	set_scope( ptrace_scope, scope );
	set_scope( ptrace_conf, conf );
#else
	(void)scope;
	(void)conf;
#endif
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
