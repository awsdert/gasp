#include "gasp.h"

int main( int argc, char *argv[] ) {
	int ret = EXIT_SUCCESS;
	int arg;
	nodes_t nodes = {0}, ARGS = {0};
	kvpair_t *args = NULL;
	char *HOME = NULL, *PWD = NULL, *DISPLAY = NULL,
		*path = NULL, *cmd = NULL;
	size_t size = 0, leng = BUFSIZ;
	if ( (ret = arguments( argc, argv, &ARGS, &leng ))
		!= EXIT_SUCCESS ) {
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
	path = calloc( leng, 1 );
	sprintf( path, "pkexec %s/%s -D HOME='%s' -D PWD='%s'",
		PWD,
#ifdef _DEBUG
		"debugger_gasp-d.elf",
#else
		"private_gasp.elf",
#endif
		HOME, PWD );
	if ( DISPLAY )
		sprintf( path, "%s -D DISPLAY='%s'", path, DISPLAY );
	for ( arg = 0; arg < argc; ++arg ) {
		cmd = argv[arg];
		if ( strstr(cmd, "gasp") ) continue;
		sprintf( path, "%s %s", path, cmd );
	}
	printf( "%s\n", path );
	if ( access( "/proc/self/exe", 0 ) == 0 )
		ret = system( path );
	else
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
	(void)less_nodes( kvpair_t, NULL, &ARGS, 0 );
	(void)less_nodes( proc_notice_t, NULL, &nodes, 0 );
	return ret;
}

