#include "gasp.h"

int set_scope( char const *path, int set ) {
	FILE *file = NULL;
	int get = 0;
	char cmdl[128] = {0};
	if ( set < 0 ) set = 0;
	if ( strstr(path,"conf") )
		sprintf( cmdl,
		"sudo echo kernel.yama.ptrace_scope = %d > '%s'", set, path );
	else
		sprintf( cmdl, "sudo echo %d > '%s'", set, path );
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

#if 0
/* Plan to simplify gasp startup with this */
int launchv( char const *app, char const *args, va_list l ) {
	int ret;
	nodes_t nodes = {0};
	char *text, *s_arg, format[bitsof(uintmax_t)] = {0}, pfx;
	intmax_t d_arg;
	uintmax_t min, u_arg;
	
	/* launch */
	if ( !app )
		return 0;
		
	if ( more_nodes( char &ret, &nodes, strlen(args) ) != 0 )
	{
		ERRMSG( ret, "Couldn't allocate enough space for arguments" );
		return ret;
	}
		
	for ( ; *args; ++args, nodes.count = strlen(text) ) {
		
		if ( *args != '%' ) {
			if ( !(text = more_nodes(	
				char, &ret, &nodes, nodes.count + 1))
			) goto fail;
			text[nodes.count] = *args;
			nodes.count++;
			continue;
		}
		++args;
		
		if ( *args = 's' ) {
			min = 0;
			pfx = ' ';
			print_str:
			va_arg( s_arg, l );
			if ( (ret = more_nodes(
				char, &nodes, nodes.count + strlen( s_arg ))) != 0
			) goto fail;
			text = nodes.space.block;
			
			if ( min )
				sprintf( format, "%%%c%jus", pfx, min );
			else
				sprintf( format, "%%s" );
			sprintf( strchr( text, 0 ), format, s_arg );
		}
		
		if ( *args == 'd' || *args == 'i' ) {
			min = 0;
			get_int:
			va_arg( d_arg, l );
			put_int:
			if ( (ret = more_nodes(	
				char, &nodes, nodes.count + bitsof(intmax_t) )) != 0
			) goto fail;
			text = nodes.space.block;
			if ( min )
				sprintf( format, "%%%c%jud", pfx, min );
			else
				sprintf( format, "%%d" );
			sprintf( strchr( text, 0 ), format, d_arg );
		}
		
		if ( *args == 'u' ) {
			min = 0;
			get_uint:
			u_arg = va_arg( uint, l );
			put_uint:
			if ( (ret = more_nodes(	
				char, &nodes, nodes.count + bitsof(intmax_t) ))
			) goto fail;
			text = nodes.space.block;
			if ( min )
				sprintf( format, "%%%c%juu", pfx, min );
			else
				sprintf( format, "%%u" );
			sprintf( strchr( text, 0 ), format, u_arg );
		}
	}
	
	fail:
	free_nodes( char, NULL, &nodes );
	return ret;
}

int launchf( char const *app, char *args, ... ) {
	int ret;
	va_start( args, l );
	ret = launchv( app, args, l );
	va_end( l );
	return ret;
}
#endif

int gasp_make_defenv_opt( nodes_t *nodes, ... ) {
	int ret = 0;
	char const *key;
	char const *val;
	char *pos;
	va_list v;
	size_t need;
	
	nodes->count = 0;
	va_start( v, nodes );
	while ( (key = va_arg( v, char const *)) )
	{
		if ( !(val = getenv(key)) )
			continue;

		need = nodes->count;
		need += strlen(key);
		need += strlen(val);
		if ( (ret = more_nodes( char, nodes, need + 10 )) != 0 )
			break;
		pos = nodes->space.block;
		
		sprintf( strchr( pos, '\0' ), " -D %s=\"%s\"", key, val );
		nodes->count += strlen(pos);
	}
	va_end(v);
	
	return ret;
}

int launch_deep_gasp(
	nodes_t *ARGS, space_t *CMDL, char const *defines
)
{
	int ret = 0;
	char const *launch, *sudo = "pkexec", *have = "HAS_ROOT_PERM",
		*PWD = getenv("PWD"), *CWD = getenv("CWD"),
		*gede = " -D USE_GEDE";
	char *cmdl, *key, *val;
	node_t i;
	size_t need;
	kvpair_t *kvpair, *kvpairs;
	
	if ( getenv(have) )
		return EALREADY;
#ifdef _DEBUG
	launch = "gasp-d.elf";
#else
	launch = "gasp.elf";
#endif

	if ( !PWD ) PWD = CWD;
	
	need = BUFSIZ;
	need += strlen(sudo) + 1; /* For '\0' */
	need += strlen(PWD) + 1; /* For '\0' */
	need += strlen(gede) + 1; /* For '\0' */
	need += strlen(launch) + 1; /* For '\0' */
	need += strlen(have) + 5; /* For " -D " '\0' */
	
	kvpairs = ARGS->space.block;
	for ( i = 0; i < ARGS->count; ++i )
	{
		kvpair = kvpairs + i;
		key = kvpair->key.block;
		val = kvpair->val.block;
		need += strlen( key ) + strlen( val ) + 3;
	}
	
	if ( (ret = more_space( CMDL, need )) != 0 )
		return ret;
	
	cmdl = CMDL->block;
	sprintf( cmdl, "%s %s/%s", sudo, PWD, launch );
	if ( g_launch_dbg )
		sprintf( strchr( cmdl,'\0' ), "%s", gede );
	sprintf( strchr( cmdl,'\0' ), " -D %s %s", have, defines );

	for ( i = 0; i < ARGS->count; ++i ) {
		kvpair = kvpairs + i;
		key = kvpair->key.block;
		val = kvpair->val.block;
		if ( key[0] == '-' && key[1] == 'D' && strstr( cmdl, val ) )
			continue;
		
		if ( val && *val )
			sprintf( strchr( cmdl, '\0'), " %s %s", key, val );
		else
			sprintf( strchr( cmdl, '\0'), " %s", key );
	}
	
	fprintf( stderr, "%s\n", cmdl );
	return system( cmdl );
}

int launch_test_gasp(
	nodes_t *ARGS, space_t *CMDL, char const *defines
)
{
	int ret = 0;
	char const *launch, *gede = "USE_GEDE",
		*PWD = getenv("PWD"), *CWD = getenv("CWD");
	char *cmdl, *key, *val;
	node_t i;
	size_t need;
	kvpair_t *kvpair, *kvpairs;
	
	if ( !g_launch_dbg )
		return EINVAL;
	launch = "gasp-d.elf";
	
	if ( !PWD ) PWD = CWD;
	
	need = BUFSIZ;
	need += strlen(launch) + 1; /* For '\0' */
	need += strlen(PWD) + 1; /* For '\0' */
	need += strlen(gede) + 5; /* For " -D " '\0' */
	
	kvpairs = ARGS->space.block;
	for ( i = 0; i < ARGS->count; ++i )
	{
		kvpair = kvpairs + i;
		key = kvpair->key.block;
		val = kvpair->val.block;
		need += strlen( key ) + strlen( val ) + 3;
	}
	
	if ( (ret = more_space( CMDL, need )) != 0 )
		return ret;
	
	cmdl = CMDL->block;
	sprintf( cmdl, "gede --args %s/%s %s", PWD, launch, defines );

	for ( i = 0; i < ARGS->count; ++i ) {
		kvpair = kvpairs + i;
		key = kvpair->key.block;
		val = kvpair->val.block;
		if ( key[0] == '-' && key[1] == 'D' && strstr( cmdl, val ) )
			continue;
		
		if ( val && *val )
			sprintf( strchr( cmdl, '\0'), " %s %s", key, val );
		else
			sprintf( strchr( cmdl, '\0'), " %s", key );
	}
	
	/* Prevent infinite loop */
	key = strstr( cmdl, " -D USE_GEDE" );
	for ( i = 0; i < 12; ++i ) key[i] = ' ';
	
	fprintf( stderr, "%s\n", cmdl );
	return system( cmdl );
}

int launch_lua() {
	int ret = 0;
	nodes_t nodes = {0};
	int scope, conf;
	char const *ptrace_scope = "/proc/sys/kernel/yama/ptrace_scope",
		*ptrace_conf = "/etc/sysctl.d/10-ptrace.conf";
	char *HOME = NULL, *path = NULL, *PWD = NULL, 
		*LUA_PATH = NULL, *LUA_CPATH = NULL, *GASP_PATH = NULL;
	size_t leng = BUFSIZ;
#if 0
	lua_CFunction old_lua_panic_cb;
#endif
	lua_State *L = NULL;
	
	scope = set_scope( ptrace_scope, 0 );
	conf = set_scope( ptrace_conf, 0 );
	
	leng += BUFSIZ;
	if ( !(HOME = getenv("HOME")) || !(PWD = getenv("PWD")) ) {
		ret = errno;
		ERRMSG( ret, "Couldn't get $(HOME) and/or $(PWD)" );
		return ret;
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
		g_reboot_gui = false;
		if ( luaL_dofile(L, path ) )
			printf("Failed:\n%s\n", lua_tostring(L,-1));
	}
	while ( g_reboot_gui );
	sprintf( path, "%s/scans", GASP_PATH );
	if ( access( path, F_OK ) == 0 ) {
		sprintf( path, "rm -r \"%s/scans\"", GASP_PATH );
		system( path );
	}
	free(path);
	path = NULL;
	lua_close(L);
	free(LUA_CPATH);
	free(LUA_PATH);
	cleanup:
#if 0
	set_scope( ptrace_scope, scope );
	set_scope( ptrace_conf, conf );
#else
	(void)scope;
	(void)conf;
#endif
	free_nodes( &nodes );
	if ( path )
		free( path );
	return ret;
}

int main( int argc, char *argv[] ) {
	int ret = EXIT_SUCCESS;
	int arg;
	nodes_t defines = {0};
	space_t PATH = {0};
	nodes_t nodes = {0}, ARGS = {0};
	kvpair_t *args = NULL;
	size_t leng = BUFSIZ;
	
	// Identify environment overrides, files to load and other things
	if ( (ret = arguments( argc, argv, &ARGS, &leng )) != 0 )
	{
		ERRMSG( ret, "Couldn't get argument pairs" );
		goto cleanup;
	}
	
	// TODO: Implement ini file based options
	// Current Groups I've thought of:
	// [Defines] [Process] [CheatFiles]
	
	if ( gasp_make_defenv_opt( &defines,
		"PWD", "CWD", "HOME", "HAS_ROOT_PERM", "USE_GEDE",
		"DISPLAY", "XDG_CURRENT_DESKTOP", "GDMSESSION",
		NULL ) != 0
	)
	{
		ERRMSG( ret, "Couldn't get environment variables" );
		goto cleanup;
	}
	
	if ( (ret = launch_deep_gasp(
		&ARGS, &PATH, defines.space.block )) == EALREADY
	)
	{
		if ( (ret = launch_test_gasp(
			&ARGS, &PATH, defines.space.block )) == EINVAL
		) ret = launch_lua();
	}
	
	cleanup:
	free_space( &PATH );
	if ( ret != 0 )
		ERRMSG( ret, "Test failed" );
	if ( args ) {
		for ( arg = 1; arg < argc; ++arg ) {
			free_space( &(args[arg].key) );
			free_space( &(args[arg].val) );
		}
	}
	free_nodes( &ARGS );
	free_nodes( &nodes );
	free_nodes( &defines );
	return (ret == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

