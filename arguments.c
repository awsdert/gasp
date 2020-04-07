#include "gasp.h"
bool g_launch_dbg = 0;
int append_to_option( option_t *option, char const *txt )
{
	int ret = 0;
	size_t
		leng = strlen(txt),
		need = option->space.given + leng + 1;
	char *text;
	
	if ( (ret = more_space( &(option->space), need )) != 0 )
		return ret;
	option->opt = option->space.block;
	text = strchr( option->opt, 0 ) + 1;
	
	REPORTF( "Copying '%s' into option block", txt )
	errno = 0;
	(void)memcpy( text, txt, leng );
	ret = errno;
	return ret;
}

int arguments( int argc, char *argv[], nodes_t *ARGS, size_t *_leng ) {
	option_t *options, *option;
	char *opt, *key, *val, c = 0;
	int begin, i, ret = 0;
	size_t leng = 1, need = 0;
	if ( _leng ) *_leng = 1;
	
	if ( (ret = more_nodes( option_t, ARGS, argc )) != 0 ) {
		ERRMSG( ret, "Couldn't allocate memory for argument pairs" );
		return ret;
	}
	options = ARGS->space.block;
	
#if 1
	begin = 1;
#else
	begin = 0;
#endif
	ARGS->count = argc - begin;
	REPORTF( "Iterating given arguments, %d", argc)
	for ( i = begin; i < argc; ++i )
	{
		option = options + (i - begin);
		opt = argv[i];
		leng = strlen(opt);
		need = leng + 5;
		
		if ( (ret = more_space( &(option->space), need )) != 0 )
		{
			ERRMSG( ret, "Not enough memory for duplicating arguments");
			return ret;
		}
		option->opt = option->space.block;
		option->key = NULL;
		option->val = NULL;
		
		REPORTF( "Copying '%s' into option block", opt )
		memcpy( option->opt, opt, leng );
		opt = option->opt;
		
		REPORTF("%s, opt = %p, leng = %zu",
			"Checking if option is a define", opt, leng)
		if ( !strstr(opt,"-D") )
			continue;
		
		if ( leng > 2 )
		{
			REPORT("Splitting key from option string")
			key = opt + 2;
			memmove( key + 1, key, strlen(key) );
			*key = 0;
			option->key = ++key;
		}
		else
		{
			REPORT("Appending key to option memory seperated by nil")
			key = argv[++i];
			if ( (ret = append_to_option( option, key )) != 0 )
				return ret;
			opt = option->opt;
			key = option->key = strchr( opt, 0 ) + 1;
		}
		
		val = strstr( key, "=" );
		if ( val )
		{
			REPORT("Splitting val from key string")
			*val = 0;
			option->val = ++val;
		}
		else
		{
			REPORT("Appending val to option memory seperated by nil")
			if ( i == (argc - 1) || argv[i+1][0] == '-' )
			{
				setenv( key, "1", 1 );
				continue;
			}
			
			val = argv[++i];
			if ( (ret = append_to_option( option, key )) != 0 )
				return ret;
			opt = option->opt;
			key = option->key = strchr( opt, 0 ) + 1;
			val = option->val = strchr( key, 0 ) + 1;
		}
		
		switch ( (c = *val) ) {
		case '"': case '\'':
			++val;
			leng = strlen(val);
			if ( val[leng-1] != c )
				break;
			val[leng-1] = 0;
		default:
			continue;
		} 
		
		for ( ++i; i < argc; ++i )
		{
			leng = strlen(argv[i]);
			need += leng;
			if ( (ret = more_space( &(option->space), need)) != 0 )
				return ret;
			memcpy( strchr( val, 0 ), argv[i], leng );
		}
	}
	
	REPORT("Done with given arguments")
	if ( _leng ) *_leng = leng;
	return ret;
}
