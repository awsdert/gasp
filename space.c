#include "gasp.h"
int change_space( space_t *space, size_t want, int dir ) {
	int ret = 0;
	uchar *block;
	/* Known variables, size, prev_size, old_top, top */
	size_t padding = (sizeof(size_t) * 2) + (sizeof(void*) * 2);
	if ( !space ) {
		if ( !want )
			return 0;
		ret = EDESTADDRREQ;
		ERRMSG( ret, "Need somewhere to place allocated memory" );
		return ret;
	}
	
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
	if ( !want )
	{
		if ( space->block )
			free( space->block );
		space->block = NULL;
		space->given = 0;
		return 0;
	}

	/* Align end of memory correctly */
	want = (((want / sizeof(int)) + 1) * sizeof(int));
	
	/* Prevent overwrites caused by poorly written realloc/malloc */
	want += padding;
	
	if ( !(space->block) )
	{
		space->block = NULL;
		space->given = 0;
		errno = 0;
		if ( !(block = malloc( want )) )
		{
			ret = errno;
			return ret ? ret : ENOMEM;
		}
		goto done;
	}
	
	errno = 0;
	if ( !(block = realloc( (space->block), want + padding )) ) {
		ret = errno;
		return ret ? ret : ENOMEM;
	}
	
	done:
	/* Do this 1st in case a thread tries to access it */
	space->block = block;
	
	/* Avoid overwriting system variables */
	want -= padding;
	
	if ( space->given < want )
		(void)memset( block + space->given, 0, want - space->given );
	
	/* Now it is safe to overwrite size */
	space->given = want;
	
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
