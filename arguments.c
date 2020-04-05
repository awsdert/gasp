#include "gasp.h"
bool g_launch_dbg = 0;
int arguments( int argc, char *argv[], nodes_t *ARGS, size_t *_leng ) {
	kvpair_t *args, *arg;
	char *cmd, *key, *val, c = 0;
	int i, ret = 0;
	size_t leng = 1;
	if ( _leng ) *_leng = 1;
	if ( (ret = more_nodes( kvpair_t, ARGS, argc )) != 0 ) {
		ERRMSG( ret, "Couldn't allocate memory for argument pairs" );
		return ret;
	}
	args = ARGS->space.block;
	
	for ( i = 1; i < argc; ++i ) {
		arg = args + i;
		cmd = argv[i];
		leng += strlen(cmd) + 1;
		if ( strcmp( cmd, "-D" ) == 0 ) {
			if ( cmd[2] )
				key = cmd + 2;
			else {
				key = argv[++i];
				leng += strlen(key) + 2;
			}
			val = strstr( key, "=" );
			if ( !val ) {
				if ( strcmp( key, "USE_GEDE" ) == 0 )
					g_launch_dbg = 1;
				else
					setenv( key, "1", 1 );
				continue;
			}
			*val = 0;
			++val;
			if ( !(*val) )
			{
				if ( strcmp( key, "USE_GEDE" ) != 0 )
					setenv( key, "0", 1 );
				--val;
				*val = '=';
				continue;
			}
			switch ( *val ) {
			case '"': case '\'':
				c = *val;
				++val;
				val[strlen(val)-1] = 0;
			}
			if ( strcmp( key, "USE_GEDE" ) == 0 )
			{
				g_launch_dbg = 1;
				continue;
			}
			else if ( (ret = change_kvpair(
				arg, 0, strlen(key) + 1, strlen(val) + 1, 1)) == 0 )
			{
				(void)strcpy( (char*)(arg->key.block), key );
				(void)strcpy( (char*)(arg->val.block), val );
				(void)setenv(
					(char*)(arg->key.block),
					(char*)(arg->val.block), 1 );
			}
			else
				ERRMSG( ret, "Couldn't create key & value pair" );
			switch ( c ) {
				case '"': case '\'':
				--val;
				*val = c;
				val[strlen(val)] = c;
			}
			--val;
			*val = '=';	
		}
	}
	if ( _leng ) *_leng = leng;
	return ret;
}
