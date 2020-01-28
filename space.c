#include "gasp.h"
void* change_space( int *err, space_t *space, size_t want, int dir ) {
	uchar *block;
	errno = EXIT_SUCCESS;
	if ( !space ) {
		if ( err ) *err = EDESTADDRREQ;
		return NULL;
	}
	if ( want % 16 ) want += (16 - (want % 16));
	if ( want == space->given ) {
		if ( err ) *err = EXIT_SUCCESS;
		return space->block;
	}
	if ( dir < 0 && want > space->given ) {
		if ( err ) *err = ERANGE;
		return NULL;
	}
	if ( dir > 0 && want < space->given ) {
		if ( err ) *err = EXIT_SUCCESS;
		return NULL;
	}
	if ( !want ) {
		if ( space->block ) free( space->block );
		space->given = 0;
		return (space->block = NULL);
	}
	if ( !(space->block) ) {
		space->block = malloc( want );
		if ( !(space->block) ) {
			if ( err ) *err = errno;
			space->given = 0;
			return NULL;
		}
		if ( err ) *err = EXIT_SUCCESS;
		(void)memset( space->block, 0, want );
		space->given = want;
		return space->block;
	}
	block = realloc( space->block, want );
	if ( !block ) {
		if ( err ) *err = errno;
		return NULL;
	}
	if ( space->given < want )
		(void)memset( block + space->given, 0, want - space->given );
	if ( err ) *err = EXIT_SUCCESS;
	space->given = want;
	return (space->block = block);
}
int change_kvpair(
	kvpair_t *kvpair,
	size_t want4full, size_t want4key, size_t want4val, int dir )
{
	int ret = EXIT_SUCCESS;
	if ( !change_space( &ret, &(kvpair->full), want4full, dir )
		&& ret != EXIT_SUCCESS )
		return ret;
	if ( !change_space( &ret, &(kvpair->key), want4key, dir )
		&& ret != EXIT_SUCCESS )
		return ret;
	(void)change_space( &ret, &(kvpair->val), want4val, dir );
	return ret;
}
