#include "gasp.h"
bool g_launch_dbg = 0;
int append_to_option(
	option_t *option, char const *set, char const *txt
)
{
	int ret = 0;
	_Bool
		setopt = ( strcmp(set, "opt") == 0 ),
		setkey = ( strcmp(set, "key") == 0 ),
		setval = ( strcmp(set, "val") == 0 );
	char
		*opt = option->opt,
		*key = option->key,
		*val = option->val;
	size_t
		leng = strlen(txt),
		need = leng + 1;
	
	if ( setval || setkey )
	{
		if ( !opt )
			return EINVAL;
		need += strlen(opt) + 1;
		if ( setval )
		{
			if ( !key )
				return EINVAL;
			need += strlen(key) + 1;
		}
	}
	
	if ( (ret = more_space( &(option->space), need )) != 0 )
		return ret;
		
	need = option->space.given;
	opt = option->opt = option->space.block;
	if ( setopt )
	{
		(void)memset( opt, 0, need );
		(void)memcpy( opt, txt, leng + 1 );
	}
	
	need -= (strlen( opt ) + 1);
	key = option->key = strchr( option->opt, 0 ) + 1;
	if ( setkey )
	{
		(void)memset( key, 0, need );
		(void)memcpy( key, txt, leng );
	}
	*(--key) = 0;
	
	need -= (strlen( key ) + 1);
	val = option->val = strchr( option->key, 0 ) + 1;
	if ( setval )
	{
		(void)memset( val, 0, need );
		(void)memcpy( val, txt, leng );
	}
	*(--val) = 0;
	
	return ret;
}

int arguments( int argc, char *argv[], nodes_t *ARGS, size_t *_leng ) {
	option_t *options, *option;
	char *opt, *key, *val, c = 0, *tmp;
	int begin, i, ret = 0;
	size_t leng = 1;
	
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
	REPORTF( "Iterating given arguments, %d", argc)
	for ( i = begin; i < argc; ++i )
	{
		option = options + ARGS->count;
		ARGS->count++;
		opt = argv[i];
		leng = strlen(opt);
	
		REPORTF( "Duplicating '%s'", opt )
		if ( (ret = append_to_option( option, "opt", opt )) != 0 )
			return ret;
		
		if ( !strstr(opt,"-D") )
			continue;
		
		key = opt[2] ? opt + 2 : argv[++i];
		tmp = strstr( key, "=" );
		if ( tmp ) *tmp = 0;
		
		if ( (ret = append_to_option( option, "key", key )) != 0 )
			return ret;
	
		if ( tmp )
		{
			*tmp = '=';
			val = tmp + 1;
		}
		else if ( i == (argc - begin) || argv[i+1][0] == '-' )
			val = "1";
		else
			val = argv[++i];
		
		if ( (ret = append_to_option( option, "val", val )) != 0 )
			return ret;
		
		val = option->val;
		switch ( (c = *val) )
		{
		case '"': case '\'':
			leng = strlen(val);
			if ( val[leng-1] != c )
				return EINVAL;
			val[leng-1] = 0;
			(void)memmove( val, val + 1, leng - 1 );
		} 
		
		(void)setenv( option->key, option->val, 1 );
	}
	
	REPORT("Done with given arguments")
	if ( _leng ) *_leng = leng;
	return ret;
}
