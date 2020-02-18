#include "gasp.h"

void error_cb( char *from, int code, char const *desc ) {
	fprintf( stderr, "%s: Error %08X\n"
		"External Msg: '%s'\n"
		"Internal Msg: '%s'\n",
		from, code, strerror(code), desc );
}

int set_scope( char const *path, int set ) {
	FILE *file = NULL;
	int get = 0;
	char cmdl[128] = {0};
	if ( set < 0 ) set = 0;
	if ( strstr(path,"conf") )
		sprintf( cmdl,
		"echo kernel.yama.ptrace_scope = %d > '%s'", set, path );
	else
		sprintf( cmdl, "echo %d > '%s'", set, path );
	if ( access( path, F_OK ) == 0 ) {
		if ( !(file = fopen(path,"r")) ) {
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
		fclose( file );
		file = NULL;
	}
	if ( set < get ) {
		fprintf( stderr, "%s:1:0: ptrace scope is restrained by"
		" value %d,\nAttempting to override to %d, "
		"Do this manually if gasp fails\n", path, get, set );
	}
	else if ( set > get ) {
		fprintf( stderr, "%s:1:0: Attempting to restore %d"
				", manually restore if fail!\n", path, set );
	}
	fprintf( stderr, "%s\n", cmdl );
	system(cmdl);
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
	path = calloc( leng, 1 );
	sprintf( path, "%s/lua/gasp.lua", PWD );
	do {
		if ( luaL_dofile(L, path ) )
			printf("Failed:\n%s\n", lua_tostring(L,-1));
	}
	while ( g_reboot_gui );
	free(path);
	path = NULL;
	lua_close(L);
	free(LUA_CPATH);
	free(LUA_PATH);
	(void)into;
	(void)i;
	(void)a;
	(void)count;
	(void)noticed;
	(void)handle;
	(void)gasp;
	(void)addr;
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
