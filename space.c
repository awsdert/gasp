#include "gasp.h"
void* change_space( int *err, space_t *space, size_t want, int dir ) {
	int ret = errno = EXIT_SUCCESS;
	uchar *block;
	if ( !space ) {
		if ( !want ) {
			released:
			if ( err ) *err = ret;
			return NULL;
		}
		ret = EDESTADDRREQ;
		if ( err ) *err = ret;
		ERRMSG( ret, "Need somewhere to place allocated memory" );
		return NULL;
	}
	if ( want % 16 ) want += (16 - (want % 16));
	if ( want == space->given ) {
		if ( err ) *err = EXIT_SUCCESS;
		return space->block;
	}
	/* Shrink */
	if ( dir < 0 && want > space->given ) {
		ret = ERANGE;
		if ( err ) *err = ret;
		ERRMSG( ret, "Tried to grow shrink-only memory" );
		return NULL;
	}
	/* Expand */
	if ( dir > 0 && want < space->given ) {
		if ( err ) *err = EXIT_SUCCESS;
		return space->block;
	}
	/* Release */
	if ( !want ) {
		if ( space->block ) free( space->block );
		space->block = NULL;
		space->given = 0;
		goto released;
	}
	if ( !(space->block) ) {
		space->block = malloc( want );
		if ( !(space->block) ) {
			space->given = 0;
			goto failed;
		}
		if ( err ) *err = ret;
		(void)memset( space->block, 0, want );
		space->given = want;
		return space->block;
	}
	block = realloc( space->block, want );
	if ( !block ) {
		failed:
		if ( errno == EXIT_SUCCESS ) errno = ENOMEM;
		ret = errno;
		if ( err ) *err = errno;
		return NULL;
	}
	if ( space->given < want )
		(void)memset( block + space->given, 0, want - space->given );
	if ( err ) *err = ret;
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
