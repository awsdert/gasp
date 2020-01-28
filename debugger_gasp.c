#include "gasp.h"
int main( int argc, char *argv[] ) {
	nodes_t ARGS = {0};
	kvpair_t *args = NULL, *arg;
	char *path = NULL, *HOME, *PWD;
	size_t leng = 0;
	int ret = EXIT_SUCCESS, i;
	if ( (ret = arguments( argc, argv, &ARGS, &leng )) != EXIT_SUCCESS ) {
		ERRMSG( ret, "Couldn't get argument pairs" );
		goto cleanup;
	}
	leng += BUFSIZ;
	args = ARGS.space.block;
	if ( !(HOME = getenv("HOME")) || !(PWD = getenv("PWD")) ) {
		ret = EINVAL;
		ERRMSG( ret, "Couldn't get $(HOME) and/or $(PWD)" );
		goto cleanup;
	}
	if ( !(path = calloc( leng, 1 )) ) {
		ret = errno;
		ERRMSG( ret, "Couldn't allocate memory to launch "
			"private_gasp.elf with");
		goto cleanup;
	}
	sprintf(path,"gede --args %s/private_gasp-d.elf", PWD);
	printf( "%s\n", path );
	ret = system( path );
	cleanup:
	if ( path ) free( path );
	if ( ret != EXIT_SUCCESS ) {
		ERRMSG( ret, "Test failed" );
		ret = EXIT_FAILURE;
	}
	if ( args ) {
		for ( i = 1; i < argc; ++i ) {
			arg = args + i;
			less_space( NULL, &(arg->key), 0 );
			less_space( NULL, &(arg->val), 0 );
		}
	}
	(void)less_nodes( kvpair_t, NULL, &ARGS, 0 );
	return ret;
}
