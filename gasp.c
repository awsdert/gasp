#include "gasp.h"

typedef enum TEXT_ID {
	TEXT_ID_SHARED = 0,
	TEXT_ID_CMD_LINE,
	TEXT_ID_GASP_PATH,
	TEXT_ID_LUA_PATH,
	TEXT_ID_LUA_CPATH,
	TEXT_ID_COUNT
} TEXT_ID_t;

int command( int *_sig, char const *cmdl, bool print_command )
{
	int ret = 0, sig = -1;
	char *pof;
	
	pof = "cmdl check";
	if ( !cmdl )
	{
		ret = EINVAL;
		goto fail;
	}
	
	if ( print_command )
		EOUTF( "%s", cmdl );
	
	errno = 0;
	sig = system(cmdl);
	ret = errno;
	
	if ( sig != 0 )
	{
		if ( ret == 0 )
			ret = ENODATA;
		goto fail;
	}
	
	if ( ret != 0 )
	{
		fail:
		FAILED( ret, pof );
	}
	
	if ( _sig )
		*_sig = sig;
	
	return ret;
}

int set_scope( char const *path, int set, int *prv ) {
	FILE *file = NULL;
	int ret = 0;
	int get = 0;
	char cmdl[128] = {0};
	
	if ( set < 0 )
		set = 0;
	
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
	
	ret = command( NULL, cmdl, 1 );
	if ( prv )
		*prv = get;
	return ret;
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
		
	pof = "character block allocation";
	if ( (ret = more_nodes( char, &nodes, strlen(args) )) != 0 )
		goto fail;
		
	for ( ; *args; ++args, nodes.count = strlen(text) ) {
		
		if ( *args != '%' ) {
			pof = "character allocation";
			if ( (ret = more_nodes(
				char, &nodes, nodes.count + 1)) != 0
			) goto fail;
			text = nodes.space.block;
			text[nodes.count] = *args;
			nodes.count++;
			continue;
		}
		++args;
		
		if ( *args = 's' )
		{
			min = 0;
			pfx = ' ';
			print_str:
			va_arg( s_arg, l );
			pof = "string allocation";
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
		
		if ( *args == 'd' || *args == 'i' )
		{
			min = 0;
			get_int:
			va_arg( d_arg, l );
			put_int:
			pof = "integer allocation";
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
			pof "unsigned integer allocation";
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
	
	if ( ret != 0 )
	{
		fail:
		FAILED( ret, pof );
	}
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

int gasp_make_defenv_opt( nodes_t *ARGS, ... ) {
	int ret = 0;
	option_t *options, *option;
	char const *key, *val, *pof;
	node_t i;
	va_list v;
	
	va_start( v, ARGS );
	
	pof = "ARGS check";
	if ( !ARGS )
	{
		ret = EDESTADDRREQ;
		goto fail;
	}
	
	while ( (key = va_arg( v, char const *)) )
	{
		if ( !(val = getenv(key)) )
		{
			if ( strcmp( key, "PWD" ) == 0 )
				val = getenv("CWD");
			else if ( strcmp( key, "CWD" ) == 0 )
				val = getenv("PWD");
			if ( !val )
				continue;
		}
		
		options = ARGS->space.block;
		for ( i = 0; i < ARGS->count; ++i )
		{
			option = options + i;
			
			if ( !(option->opt) || !(option->key) || !(option->val) )
			{
				if ( option->opt )
					EOUTF( "opt = '%s'", option->opt );
				if ( option->key )
					EOUTF( "key = '%s'", option->key );
				if ( option->val )
					EOUTF( "val = '%s'", option->val );
				ret = EINVAL;
				goto fail;
			}
			
			if ( strcmp( option->opt, "-D" ) != 0 )
				continue;
				
			if ( strcmp( option->key, key ) == 0 )
				break;
		}
		
		if ( i < ARGS->count )
			continue;
		
		if ( (ret = more_nodes(
			option_t, ARGS, ARGS->count + 1 )) != 0
		) break;
		options = ARGS->space.block;
		option = options + i;
		
		if ( (ret = append_to_option( option, "opt", "-D" )) != 0 )
			return ret;
		
		if ( (ret = append_to_option( option, "key", key )) != 0 )
			return ret;
		
		if ( (ret = append_to_option( option, "val", val )) != 0 )
			return ret;
		
		ARGS->count++;
	}
	
	if ( ret != 0 )
	{
		fail:
		FAILED( ret, pof );
	}
	va_end(v);
	return ret;
}

int find_option( nodes_t *ARGS, char const *opt, node_t *pos )
{
	int ret = 0;
	node_t i;
	option_t *option, *options = ARGS->space.block;
	text_t TEXT = {0};
	char *txt, *key, *pof;
	size_t size = strlen(opt) + 1;
	
	pof = "more characters";
	if ( (ret = more_nodes( char, &TEXT, size )) != 0 )
		goto fail;
		
	txt = TEXT.space.block;
	(void)memcpy( txt, opt, size );
	
	key = strchr( (opt = txt), ' ' );
	if ( key )
		*(key++) = 0;
	
	for ( i = 0; i < ARGS->count; ++i )
	{
		option = options + i;
		if ( strcmp( option->opt, opt ) == 0 )
		{
			if ( !key || strcmp( option->key, key ) == 0 )
			{
				ret = EEXIST;
				goto done;
			}
		}
	}
	i = ~0;
	
	done:
	
	switch ( ret )
	{
	case 0: case EEXIST: break;
	default:
		fail:
		FAILED( ret, pof );
	}
	
	if ( pos ) *pos = i;
	return ret;
}

int append_options( text_t *TEXTV, nodes_t *ARGS )
{	
	int ret = 0;
	node_t i;
	text_t
		/* Repurpose for */
		*TEXT = TEXTV + TEXT_ID_SHARED,
		*CMDL = TEXTV + TEXT_ID_CMD_LINE;
	size_t need = CMDL->space.given, want = 0;
	char *cmdl = CMDL->space.block, *opt, *key, *val, *text;
	option_t *option, *options = ARGS->space.block;
	
	for ( i = 0; i < ARGS->count; ++i )
	{
		option = options + i;
		need += option->space.given;
		if ( option->space.given > want )
			want = option->space.given;
	}
	
	want += 10;
	if ( (ret = more_nodes( char, TEXT, want )) != 0 )
		return ret;
	text = TEXT->space.block;
	
	if ( (ret = more_nodes( char, CMDL, need * 2 )) != 0 )
		return ret;
	cmdl = CMDL->space.block;

	for ( i = 0; i < ARGS->count; ++i )
	{
		option = options + i;
		opt = option->opt;
		key = option->key;
		val = option->val;
		
		(void)memset( text, 0, want ); 
		sprintf( text, "%s", opt );
		
		if ( strcmp( opt, "-D" ) == 0 )
			sprintf( strchr( text, 0 ), " %s=\"%s\"", key, val );
		
		if ( !strstr( cmdl, text ) )
			sprintf( strchr( cmdl, 0 ), " %s", text );
	}
	
	return 0;
}

int append_path_to_self( text_t *TEXT, bool use_proc_pid_exe )
{
	int ret = 0;
	char *text, *pof, path[PID_PATH] = {0};
	char const *launch = NULL,
		*PWD = getenv("PWD"),
		*CWD = getenv("CWD"),
		*CD = getenv("CD"),
		*_CWD = getenv("CURRENT_WORKING_DIRECTORY");
	size_t need = 0;
	
	pof = "TEXT";
	if ( !TEXT )
	{
		ret = EDESTADDRREQ;
		goto fail;
	}
	
#ifdef _DEBUG
	launch = "gasp-d.elf";
#else
	launch = "gasp.elf";
#endif
	
	if ( !PWD ) PWD = CWD;
	if ( !PWD ) PWD = CD;
	if ( !PWD ) PWD = _CWD;
	
	if ( !PWD || use_proc_pid_exe )
	{
		sprintf( path, "/proc/%d", getpid() );
		PWD = path;
		launch = "exe";
	}
	
	need = TEXT->count + strlen(PWD) + strlen(launch) + 2;
	
	pof = "character allocation";
	if ( (ret = more_nodes( char, TEXT, need )) != 0 )
		goto fail;
	
	text = TEXT->space.block;
	pof = "sprintf";
	errno = 0;
	if ( sprintf( strchr( text, 0 ), "%s/%s", PWD, launch ) <= 0 )
	{
		ret = errno;
		fail:
		FAILED( ret, pof );
		EOUTF( "PWD = '%s'", PWD ? PWD : "(null)" );
		EOUTF( "CWD = '%s'", CWD ? CWD : "(null)" );
		EOUTF( "CD = '%s'", CD ? CD : "(null)" );
		EOUTF( "CURRENT_WORKING_DIRECTORY = '%s'",
			_CWD ? _CWD : "(null)" );
	}
	return ret;
}

int launch_sudo_gasp( text_t *TEXTV, nodes_t *ARGS )
{
	int ret = 0, sig = 0;
	char const *sudo = "pkexec", *opt = "--inrootmode", *pof;
	char *cmdl;
	text_t *CMDL = TEXTV + TEXT_ID_CMD_LINE;
	size_t need;
	
	pof = "find option";
	switch ( (ret = find_option( ARGS, opt, NULL )) )
	{
	case 0: break;
	case EEXIST: return ret;
	default:
		goto fail;
	}
	
	need = strlen(sudo) + 2; /* For ' ' & '\0' */
	
	pof = "allocate characters for pkexec";
	if ( (ret = more_nodes( char, CMDL, need )) != 0 )
		goto fail;
	cmdl = CMDL->space.block;
	
	sprintf( cmdl, "%s ", sudo );
	
	pof = "append path to self";
	if ( (ret = append_path_to_self( CMDL, 1 )) != 0 )
		goto fail;
		
	pof = "append characters for declaring root";
	if ( (ret = more_nodes( char, CMDL, strlen(opt) + 2 )) != 0 )
		goto fail;
	cmdl = CMDL->space.block;
		
	(void)sprintf( strchr( cmdl, 0 ), " %s",  opt );
	
	pof = "append options";
	if ( (ret = append_options( TEXTV, ARGS )) != 0 )
		goto fail;
	cmdl = CMDL->space.block;
	
	pof = "launch self";
	if ( (ret = command( &sig, cmdl, 1 )) != 0 )
		goto fail;
	
	if ( ret != 0 )
	{
		fail:
		FAILED( ret, pof );
		EOUTF( "sig = %d", sig );
	}
	
	return ret;
}

int launch_test_gasp( text_t *TEXTV, nodes_t *ARGS )
{
	int ret = 0, sig = 0;
	char const *gede = "gede --args ", *opt = "--gede";
	char *cmdl, *pos, *nxt, *pof;
	text_t *CMDL = TEXTV + TEXT_ID_CMD_LINE;
	size_t need;
	
	pof = "find option";
	switch ( (ret = find_option( ARGS, opt, NULL )) )
	{
	case 0: return 0;
	case EEXIST: break;
	default:
		goto fail;
	}
	
	/* For ' ' & '\0' */
	need = strlen(gede) + 2;
	
	pof = "allocate characters for launching gede with arguments";
	if ( (ret = more_nodes( char, CMDL, need )) != 0 )
		return ret;
	cmdl = CMDL->space.block;
	
	sprintf( cmdl, "%s", gede );
	
	pof = "append path to self";
	if ( (ret = append_path_to_self( CMDL, 1 )) != 0 )
		goto fail;

	pof = "append options";
	if ( (ret = append_options( TEXTV, ARGS )) != 0 )
		goto fail;
	cmdl = CMDL->space.block;
	
	/* Prevent infinite loop */
	pos = strstr( cmdl, opt );
	if ( pos )
	{
		nxt = pos + strlen( opt );
		(void)memmove( pos, nxt, strlen(nxt) + 1 );
	}
	else
	{
		ret = EDESTADDRREQ;
		goto fail;
	}
	
	pof = "launch self";
	if ( (ret = command( &sig, cmdl, 1 )) != 0 )
		goto fail;
	
	if ( ret != 0 )
	{
		fail:
		FAILED( ret, pof );
		EOUTF( "sig = %d", sig );
	}
	return ret;
}

int launch_lua( text_t *TEXTV ) {
	int ret = 0, scope = 1, conf = 1;
	char const *pof,
		*ptrace_scope = "/proc/sys/kernel/yama/ptrace_scope",
		*ptrace_conf = "/etc/sysctl.d/10-ptrace.conf";
	char *paths[TEXT_ID_COUNT] = {NULL}, *HOME = NULL, *PWD = NULL;
	size_t leng;
	node_t i;
#if 0
	lua_CFunction old_lua_panic_cb;
#endif
	lua_State *L = NULL;
	
	pof = "set scope via /proc";
	if ( (ret = set_scope( ptrace_scope, 0, &scope )) != 0 )
		goto fail;
	
	pof = "set scope via /etc";
	if ( (ret = set_scope( ptrace_conf, 0, &conf )) != 0 )
		goto fail;
	
	pof = "environment variables check";
	if ( !(PWD = getenv("PWD")) ) PWD = getenv("CWD");
	
	if ( !(HOME = getenv("HOME")) || !PWD )
	{
		ret = ENODATA;
		goto fail;
	}
	
	leng = strlen(PWD) + BUFSIZ + strlen(HOME);
	for ( i = 0; i < TEXT_ID_COUNT; ++i )
	{
		pof = "alloc memory for shared text";
		if ( (ret = more_nodes( char, TEXTV + i, leng )) != 0 )
			goto fail;
		paths[i] = TEXTV[i].space.block;
	}
	
	pof = "init lua";
	errno = 0;
	if ( !(L = luaL_newstate()) )
	{
		ret = errno;
		goto fail;
	}
	lua_atpanic(L,lua_panic_cb);
	
	/* Just a hack for slipups upstream */
	(void)luaL_dostring( L,
		"if not _G.loadlib then\n"
			"_G.loadlib = package.loadlib\n"
		"end"
	);
	
	/* Load in extras */
	luaL_openlibs(L);
	lua_create_gasp(L);
	
	/* Run lua */
	sprintf( paths[TEXT_ID_SHARED],
		"%s/lua/gasp.lua", getenv("GASP_PATH") );
	do {
		g_reboot_gui = false;
		if ( luaL_dofile(L, paths[TEXT_ID_SHARED] ) )
			printf("Failed:\n%s\n", lua_tostring(L,-1));
	}
	while ( g_reboot_gui );
	
	lua_close(L);
	
	if ( ret != 0 )
	{
		fail:
		FAILED( ret, pof );
		EOUTF( "PWD = %p, '%s'", PWD, PWD ? PWD : "(null)" );
		EOUTF( "HOME = %p, '%s'", HOME, HOME ? HOME : "(null)" );
	}
	
#if 1
	ret = set_scope( ptrace_scope, scope, NULL );
	ret = set_scope( ptrace_conf, conf, NULL );
#else
	(void)scope;
	(void)conf;
#endif

	return ret;
}

int main( int argc, char *argv[] )
{
	int ret = 0;
	text_t TEXTV[TEXT_ID_COUNT] = {{0}}, *TEXT;
	nodes_t ARGS = {0};
	option_t *options, *option;
	size_t leng = BUFSIZ;
	node_t i, pos;
	char const *PWD, *HOME;
	char *text, *pof;
	
	/* Allocate all memory need for start up */
	pof = "allocate all shared memory";
	for ( i = 0; i < TEXT_ID_COUNT; ++i )
	{
		if ( (ret = more_nodes( char, TEXTV + i, leng )) != 0 )
			goto fail;
	}
	
	// Identify environment overrides, files to load and other things
	pof = "argument capture";
	if ( (ret = arguments( argc, argv, &ARGS, &leng )) != 0 )
		goto fail;
	
	// TODO: Implement ini file based options
	// Current Groups I've thought of:
	// [Defines] [Process] [CheatFiles]
	
	pof = "environment variable defines";
	if ( (ret = gasp_make_defenv_opt( &ARGS,
		"PWD", "CWD", "HOME", "GASP_PATH", "LUA_PATH", "LUA_CPATH",
		"DISPLAY", "XDG_CURRENT_DESKTOP", "GDMSESSION",
		NULL )) != 0
	) goto fail;
	
	PWD = getenv( "PWD" );
	HOME = getenv( "HOME" );
	TEXT = TEXTV + TEXT_ID_SHARED;
	leng += strlen( HOME ) + strlen(PWD);
	pof = "allocate characters for PWD & HOME";
	if ( (ret = more_nodes( char, TEXT, leng )) != 0 )
		goto fail;
	text = TEXT->space.block;
	
	(void)memset( text, 0, TEXT->space.given );
	(void)sprintf( text, "%s/OTG", PWD );
	if ( (gasp_isdir( text, 0 )) != 0 )
	{
		(void)memset( text, 0, TEXT->space.given );
		(void)sprintf( text, "%s/.config", HOME );
		pof = "create ~/.config";
		if ( (gasp_isdir( text, 1 )) != 0 )
			goto fail;

		(void)memset( text, 0, TEXT->space.given );
		(void)sprintf( text, "%s/.config/gasp", HOME );
		pof = "create ~/.config/gasp";
		if ( (gasp_isdir( text, 1 )) != 0 )
			goto fail;
	}
	
	pof = "add define for GASP_PATH";
	if ( (ret = add_define( &ARGS, "GASP_PATH", &pos, 0 )) != EEXIST )
	{
		if ( ret != 0 )
			goto fail;
		
		options = ARGS.space.block;
		option = options + pos;
		
		pof = "append value to option object";
		if ( (ret = append_to_option( option, "val", text )) != 0 )
			goto fail;
			
		setenv( "GASP_PATH", option->val, 1 );
	}
	
	(void)memset( text, 0, TEXT->space.given );
	(void)sprintf( text, "%s/?.so", PWD );
	pof = "add define for LUA_CPATH";
	if ( (ret = add_define( &ARGS, "LUA_CPATH", &pos, 0 )) != EEXIST )
	{
		if ( ret != 0 )
			goto fail;
		
		options = ARGS.space.block;
		option = options + pos;
			
		if ( (ret = append_to_option( option, "val", text )) != 0 )
			goto fail;
			
		setenv( "LUA_CPATH", option->val, 1 );
	}
	
	(void)memset( text, 0, TEXT->space.given );
	(void)sprintf( text,
		"%s/?.lua;%s/lua/?.lua", PWD, getenv("GASP_PATH") );
	pof = "add define for LUA_PATH";
	if ( (ret = add_define( &ARGS, "LUA_PATH", &pos, 0 )) != EEXIST )
	{
		if ( ret != 0 )
			goto fail;
		
		options = ARGS.space.block;
		option = options + pos;
			
		if ( (ret = append_to_option( option, "val", text )) != 0 )
			goto fail;
			
		setenv( "LUA_PATH", option->val, 1 );
	}
	
	pof = "launch under sudo check";
	if ( (ret = launch_sudo_gasp( TEXTV, &ARGS )) == EEXIST )
	{
		pof = "launch under gasp check";
		if ( (ret = launch_test_gasp( TEXTV, &ARGS )) == 0 )
		{
			/* Don't need this so give back memory */
			free_nodes( TEXTV + TEXT_ID_CMD_LINE );
			/* Open user interface */
			pof = "launch lua";
			ret = launch_lua( TEXTV );
		}
	}
	
	if ( ret != 0 )
	{
		fail:
		FAILED( ret, pof );
	}
	
	options = ARGS.space.block;
	if ( options )
	{
		for ( i = 0; i < ARGS.count; ++i ) {
			option = options + i;
			free_space( &(option->space) );
		}
	}
	free_nodes( &ARGS );
	
	for ( i = 0; i < TEXT_ID_COUNT; ++i )
		free_nodes( TEXTV + i );
	
	return (ret == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

