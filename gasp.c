#include "gasp.h"

typedef enum TEXT_ID {
	TEXT_ID_SHARED = 0,
	TEXT_ID_CMD_LINE,
	TEXT_ID_GASP_PATH,
	TEXT_ID_LUA_PATH,
	TEXT_ID_LUA_CPATH,
	TEXT_ID_COUNT
} TEXT_ID_t;

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
	
	fprintf( stdout, "%s\n", cmdl );
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

int gasp_make_defenv_opt( nodes_t *ARGS, ... ) {
	int ret = 0;
	option_t *options, *option;
	char const *key, *val;
	node_t i;
	va_list v;
	
	va_start( v, ARGS );
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
				ERRMSG( ret, "Corrupt option" );
				return ret;
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
	va_end(v);
	
	return ret;
}

int find_option( nodes_t *ARGS, char const *opt, node_t *pos )
{
	int ret = 0;
	node_t i;
	option_t *option, *options = ARGS->space.block;
	text_t TEXT = {0};
	char *txt, *key;
	size_t size = strlen(opt) + 1;
	
	if ( (ret = more_nodes( char, &TEXT, size )) != 0 )
		goto done;
		
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
	free_nodes( &TEXT );
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

int launch_sudo_gasp( text_t *TEXTV, nodes_t *ARGS )
{
	int ret = 0;
	char const *launch, *sudo = "pkexec", *root = "--inrootmode",
		*PWD = getenv("PWD"), *CWD = getenv("CWD");
	char *cmdl;
	text_t *CMDL = TEXTV + TEXT_ID_CMD_LINE;
	size_t need;
	
	if ( (ret = find_option( ARGS, root, NULL )) != 0 )
		return ret;
	
#ifdef _DEBUG
	launch = "gasp-d.elf";
#else
	launch = "gasp.elf";
#endif

	if ( !PWD ) PWD = CWD;
	
	need = BUFSIZ;
	need += strlen(sudo) + 1; /* For '\0' */
	need += strlen(PWD) + 1; /* For '\0' */
	need += strlen(launch) + 1; /* For '\0' */
	need += strlen(root) + 2; /* For ' ' and '\0' */
	
	if ( (ret = more_nodes( char, CMDL, need )) != 0 )
		return ret;
	cmdl = CMDL->space.block;
	
	sprintf( cmdl, "%s %s/%s %s", sudo, PWD, launch, root );
	
	if ( (ret = append_options( TEXTV, ARGS )) != 0 )
		return ret;
	cmdl = CMDL->space.block;
	
	errno = 0;
	fprintf( stdout, "%s\n", cmdl );
	
	ret = system( cmdl );
	if ( errno != 0 )
		return errno;
	return ret;
}

int launch_test_gasp( text_t *TEXTV, nodes_t *ARGS )
{
	int ret = 0;
	char const *launch, *gede = "--gede",
		*PWD = getenv("PWD"), *CWD = getenv("CWD");
	char *cmdl, *pos, *nxt;
	text_t *CMDL = TEXTV + TEXT_ID_CMD_LINE;
	size_t need;
	
	if ( (ret = find_option( ARGS, gede, NULL )) != EEXIST )
		return ret;
	
	launch = "gasp-d.elf";
		
	if ( !PWD ) PWD = CWD;
	
	need = BUFSIZ;
	need += strlen(launch) + 1; /* For '\0' */
	need += strlen(PWD) + 1; /* For '\0' */
	need += strlen(gede) + 5; /* For " -D " '\0' */
	
	if ( (ret = more_nodes( char, CMDL, need )) != 0 )
		return ret;
	cmdl = CMDL->space.block;
	
	sprintf( cmdl, "gede --args ./%s", launch );

	if ( (ret = append_options( TEXTV, ARGS )) != 0 )
		return ret;
	cmdl = CMDL->space.block;
	
	/* Prevent infinite loop */
	pos = strstr( cmdl, gede );
	
	fprintf( stderr, "%s\n", cmdl );
	if ( pos )
	{
		nxt = pos + strlen( gede );
		(void)memmove( pos, nxt, strlen(nxt) + 1 );
	}
	
	errno = 0;
	ret = system( cmdl );
	if ( errno != 0 )
		return errno;
	return ret;
}

int launch_lua( text_t *TEXTV ) {
	int ret = 0, scope, conf;
	char const
		*ptrace_scope = "/proc/sys/kernel/yama/ptrace_scope",
		*ptrace_conf = "/etc/sysctl.d/10-ptrace.conf";
	char *paths[TEXT_ID_COUNT] = {NULL}, *HOME = NULL, *PWD = NULL;
	size_t leng;
	node_t i;
#if 0
	lua_CFunction old_lua_panic_cb;
#endif
	lua_State *L = NULL;
	
	scope = set_scope( ptrace_scope, 0 );
	conf = set_scope( ptrace_conf, 0 );
	
	if ( !(PWD = getenv("PWD")) ) PWD = getenv("CWD");
	
	if ( !(HOME = getenv("HOME")) || !PWD )
	{
		ret = ENODATA;
		ERRMSG( ret, "Couldn't get $(HOME) and/or $(PWD)" );
		EOUTF( "PWD = %p, '%s'", PWD, PWD ? PWD : "(null)" );
		EOUTF( "HOME = %p, '%s'", HOME, HOME ? HOME : "(null)" );
		return ret;
	}
	
	leng = strlen(PWD) + BUFSIZ + strlen(HOME);
	for ( i = 0; i < TEXT_ID_COUNT; ++i )
	{
		if ( (ret = more_nodes( char, TEXTV + i, leng )) != 0 )
		{
			ERRMSG( ret, "Unable to allocate memory for local path" );
			goto fail;
		}
		paths[i] = TEXTV[i].space.block;
	}
	
	/* Initialise lua */
	errno = 0;
	if ( !(L = luaL_newstate()) ) {
		ret = errno;
		ERRMSG( ret, "Couldn't create lua instance" );
		goto fail;
	}
	lua_atpanic(L,lua_panic_cb);
	
	/* Just a hack for slipups upstream */
	(void)luaL_dostring(L,
		"if not loadlib then\n"
			"loadlib = package.loadlib\n"
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
	
	fail:
#if 0
	set_scope( ptrace_scope, scope );
	set_scope( ptrace_conf, conf );
#else
	(void)scope;
	(void)conf;
#endif
	return ret;
}

int main( int argc, char *argv[] ) {
	int ret = EXIT_SUCCESS;
	text_t TEXTV[TEXT_ID_COUNT] = {{0}}, *TEXT;
	nodes_t ARGS = {0};
	option_t *options, *option;
	size_t leng = BUFSIZ;
	node_t i, pos;
	char const *PWD, *HOME;
	char *text;
	
	/* Allocate all memory need for start up */
	for ( i = 0; i < TEXT_ID_COUNT; ++i )
	{
		if ( (ret = more_nodes( char, TEXTV + i, leng )) != 0 )
		{
			ERRMSG( ret, "Unable to allocate memory for shared text" );
			goto fail;
		}
	}
	
	// Identify environment overrides, files to load and other things
	if ( (ret = arguments( argc, argv, &ARGS, &leng )) != 0 )
	{
		ERRMSG( ret, "Couldn't get argument pairs" );
		goto fail;
	}
	
	// TODO: Implement ini file based options
	// Current Groups I've thought of:
	// [Defines] [Process] [CheatFiles]
	
	if ( (ret = gasp_make_defenv_opt( &ARGS,
		"PWD", "CWD", "HOME", "GASP_PATH", "LUA_PATH", "LUA_CPATH",
		"DISPLAY", "XDG_CURRENT_DESKTOP", "GDMSESSION",
		NULL )) != 0
	)
	{
		ERRMSG( ret, "Couldn't get environment variables" );
		goto fail;
	}
	
	PWD = getenv( "PWD" );
	HOME = getenv( "HOME" );
	TEXT = TEXTV + TEXT_ID_SHARED;
	leng += strlen( HOME ) + strlen(PWD);
	if ( (ret = more_nodes( char, TEXT, leng )) != 0 )
		goto fail;
	text = TEXT->space.block;
	
	(void)memset( text, 0, TEXT->space.given );
	(void)sprintf( text, "%s/OTG", PWD );
	if ( (gasp_isdir( text, 0 )) != 0 )
	{
		(void)memset( text, 0, TEXT->space.given );
		(void)sprintf( text, "%s/.config", HOME );
		if ( (gasp_isdir( text, 1 )) != 0 )
			goto fail;

		(void)memset( text, 0, TEXT->space.given );
		(void)sprintf( text, "%s/.config/gasp", HOME );
		if ( (gasp_isdir( text, 1 )) != 0 )
			goto fail;
	}
	
	if ( (ret = add_define( &ARGS, "GASP_PATH", &pos, 0 )) != EEXIST )
	{
		if ( ret != 0 )
			goto fail;
		
		options = ARGS.space.block;
		option = options + pos;
			
		if ( (ret = append_to_option( option, "val", text )) != 0 )
			goto fail;
			
		setenv( "GASP_PATH", option->val, 1 );
	}
	
	(void)memset( text, 0, TEXT->space.given );
	(void)sprintf( text, "%s/?.so", PWD );
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
	
	if ( (ret = launch_sudo_gasp( TEXTV, &ARGS )) == EEXIST )
	{
		if ( (ret = launch_test_gasp( TEXTV, &ARGS )) == 0 )
		{
			/* Don't need this so give back memory */
			free_nodes( TEXTV + TEXT_ID_CMD_LINE );
			/* Open user interface */
			ret = launch_lua( TEXTV );
		}
	}
	
	fail:
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
	
	if ( ret != 0 )
		ERRMSG( ret, "Test failed" );
	
	return (ret == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

