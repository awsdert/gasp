#include "gasp.h"
int change_space( space_t *space, size_t want, int dir ) {
	int ret = errno = 0;
	uchar *block;
	if ( !space ) {
		if ( !want )
			return ret;
		ret = EDESTADDRREQ;
		ERRMSG( ret, "Need somewhere to place allocated memory" );
		return ret;
	}
	
	if ( want % 16 )	
		want += (16 - (want % 16));
	
	if ( want == space->given )
		return 0;
	
	/* Shrink */
	if ( dir < 0 && want > space->given ) {
		ret = ERANGE;
		ERRMSG( ret, "Tried to grow shrink-only memory" );
		return ret;
	}
	
	/* Expand */
	if ( dir > 0 && want < space->given )
		return 0;
	
	/* Release */
	if ( !want ) {
		if ( space->block ) free( space->block );
		space->block = NULL;
		space->given = 0;
		return 0;
	}
	
	if ( !(space->block) ) {
		if ( !(space->block = malloc( want )) )
		{
			ret = errno;
			space->given = 0;
			return ret ? ret : ENOMEM;
		}
		(void)memset( space->block, 0, want );
		space->given = want;
		return 0;
	}
	
	if ( !(block = realloc( space->block, want )) ) {
		ret = errno;
		return ret ? ret : ENOMEM;
	}
	
	/* Do this 1st in case a thread tries to access it */
	space->block = block;
	space->given = want;
	
	if ( space->given < want )
		(void)memset( block + space->given, 0, want - space->given );
	
	return 0;
}
int change_kvpair(
	kvpair_t *kvpair,
	size_t want4full, size_t want4key, size_t want4val, int dir )
{
	int ret = 0;
	
	if (
		(ret = change_space( &(kvpair->full), want4full, dir )) != 0
		|| (ret = change_space( &(kvpair->key), want4key, dir )) != 0
	) return ret;
	
	return change_space( &(kvpair->val), want4val, dir );
}
