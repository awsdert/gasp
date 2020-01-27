#include "gasp.h"

int arguments( int argc, char *argv[], nodes_t *ARGS, size_t *_leng ) {
	key_val_t *args, *arg;
	char *cmd, *key, *val;
	int i, ret = EXIT_SUCCESS;
	size_t leng = 1;
	if ( _leng ) *_leng = 1;
	if ( !(args = more_nodes( key_val_t, &ret, ARGS, argc )) ) {
		ERRMSG( ret, "Couldn't allocate memory for argument pairs" );
		return ret;
	}
	for ( i = 1; i < argc; ++i ) {
		arg = args + i;
		cmd = argv[i];
		printf( "cmd = '%s'\n", cmd );
		leng += strlen(cmd) + 1;
		if ( strcmp( cmd, "-D" ) == 0 ) {
			if ( cmd[3] )
				key = cmd + 3;
			else {
				key = argv[++i];
				leng += strlen(key) + 2;
			}
			val = strstr( key, "=" );
			*val = 0;
			++val;
			switch ( *val ) {
			case '"': case '\'':
				++val;
				val[strlen(val)-1] = 0;
			}
			printf("key = '%s', val = '%s'\n", key, val );
			if ( (ret = change_key_val(
				arg, 0, strlen(key) + 1, strlen(val) + 1))
				== EXIT_SUCCESS ) {
				(void)strcpy( (char*)(arg->key.block), key );
				(void)strcpy( (char*)(arg->val.block), val );
				(void)setenv(
					(char*)(arg->key.block),
					(char*)(arg->val.block), 1 );
			}
			else
				ERRMSG( ret, "Couldn't create key & value pair" );
			key[strlen(key)] = '=';
			--val;
			val[strlen(val)] = *val;
		}
	}
	if ( _leng ) *_leng = leng;
	return ret;
}
