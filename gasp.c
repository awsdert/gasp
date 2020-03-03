#include "gasp.h"

int main( int argc, char *argv[] ) {
	int ret = EXIT_SUCCESS;
	int arg;
	space_t PATH = {0};
	nodes_t nodes = {0}, ARGS = {0};
	kvpair_t *args = NULL;
	char *HOME = NULL, *PWD = NULL, *DISPLAY = NULL, *CWD = NULL,
		*path = NULL, *cmd = NULL, gasp[] = "gasp", *launch;
	size_t leng = BUFSIZ
#if 0
	, size = 0
#endif
	;
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
	if ( !(CWD = getenv("CWD")) ) CWD = PWD;
	leng = strlen(PWD) + 20;
	leng += strlen(HOME) + 32;
	leng += strlen(PWD) + 4;
	leng += strlen(CWD) + 4;
	leng += strlen(HOME) + 4;
	leng += strlen(DISPLAY) + 4;
	if ( !(path = more_space( &ret, &PATH, leng )) ) {
		ret = errno;
		ERRMSG( errno, "Couldn't allocate path" );
		goto cleanup;
	}
#ifdef _DEBUG
	launch = "test-gasp-d.elf";
#else
	launch = "deep-gasp.elf";
#endif
	sprintf( path, "pkexec sudo %s/%s"
		" -D HOME='%s' -D PWD='%s' -D CWD='%s'",
		PWD, launch, HOME, PWD, CWD );
	if ( DISPLAY )
		sprintf( strchr(path,'\0'), " -D DISPLAY='%s'", DISPLAY );
	for ( arg = 0; arg < argc; ++arg ) {
		cmd = argv[arg];
		if ( strstr(cmd, gasp) ) continue;
		sprintf( strchr( path, '\0'), " %s", cmd );
	}
	fprintf(stderr,"%s\n",path);
	ret = system( path );
	cleanup:
	(void)free_space( &ret, &PATH );
	if ( ret != EXIT_SUCCESS ) {
		ERRMSG( ret, "Test failed" );
		ret = EXIT_FAILURE;
	}
	if ( args ) {
		for ( arg = 1; arg < argc; ++arg ) {
			(void)free_space( NULL, &(args[arg].key) );
			(void)free_space( NULL, &(args[arg].val) );
		}
	}
	(void)free_nodes( kvpair_t, NULL, &ARGS );
	(void)free_nodes( proc_notice_t, NULL, &nodes );
	return ret;
}

