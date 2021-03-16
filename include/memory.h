#ifndef MEMORY_H
#define MEMORY_H

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <malloc.h>
#endif

struct memory_block
{
	void *block;
	size_t bytes;
};

#define get_memory_block_bytes( block ) ((block)->bytes)
#define get_memory_block_block( T, block ) ((T)((block)->block))

/* Initialises memory block structure
 * 
 * @memory_block Pointer to structure you want to initialise, will segfault on
 * invalid pointer
 * 
 * @bytes Number of bytes you want to start with
 * 
 * Will attempt to allocate block of bytes to point the structure to
 * 
 * @return NULL on failure, valid pointer on success
*/
void * new_memory_block( struct memory_block *memory_block, size_t bytes );

/* Reallocates memory pointed by structure
 * 
 * @memory_block Pointer to structure you want to reallocate pointed memory of,
 * will segfault on invalid pointer
 * 
 * @bytes New number of bytes to allocate
 * 
 * Will attempt to reallocate block pointed to by structure and update the
 * members on success, on failure it is left as is, attempts with invalid
 * pointer in memory_block->block can lead to undefined behaviour, currently
 * whatever realloc() does in this situation is what happens here as it is
 * passed directly onto the function
 * 
 * @return NULL on failure, memory_block->block on success
*/
void * alt_memory_block( struct memory_block *memory_block, size_t bytes );

/* Reallocates memory pointed by structure
 * 
 * @memory_block Pointer to structure you want to reallocate pointed memory of,
 * will segfault on invalid pointer
 * 
 * @bytes New number of bytes to allocate
 * 
 * Will attempt to reallocate block pointed to by structure ONLY if the wanted
 * bytes are more than the current amount allocated and update the
 * members on success, on failure it is left as is, attempts with invalid
 * pointer in memory_block->block can lead to undefined behaviour, currently
 * whatever realloc() does in this situation is what happens here as it is
 * passed directly onto the function
 * 
 * @return NULL on failure, memory_block->block on success
*/
void * inc_memory_block( struct memory_block *memory_block, size_t bytes );

/* Reallocates memory pointed by structure
 * 
 * @memory_block Pointer to structure you want to reallocate pointed memory of,
 * will segfault on invalid pointer
 * 
 * @bytes New number of bytes to allocate
 * 
 * Will attempt to reallocate block pointed to by structure ONLY if the wanted
 * bytes are less than the current amount allocated and update the
 * members on success, on failure it is left as is, attempts with invalid
 * pointer in memory_block->block can lead to undefined behaviour, currently
 * whatever realloc() does in this situation is what happens here as it is
 * passed directly onto the function
 * 
 * @return NULL on failure, memory_block->block on success
*/
void * dec_memory_block( struct memory_block *memory_block, size_t bytes );

/* Releases pointed memory
 * 
 * @memory_block Pointer to memory block structure holding a pointer to the
 * block to be released, will segfault on invalid pointer
 * 
 * Will attempt to release memory pointed to by memory_block->block, afterwards
 * it will reinitialise the members to 0 & NULL, invalid pointers can lead to
 * undefined behaviour, currently whatever free() does in this situation is
 * what happens here as the pointer is directly passed on to the function
*/
void del_memory_block( struct memory_block *memory_block );

struct memory_group
{
	struct memory_block memory_block;
	int total, count, focus;
};

#define get_memory_group_total( group ) ((group)->total)
#define get_memory_group_count( group ) ((group)->count)
#define get_memory_group_focus( group ) ((group)->focus)
#define get_memory_group_bytes( group ) \
	get_memory_block_bytes( &((group)->memory_block) )
#define get_memory_group_group( T, group ) \
	get_memory_block_block( T*, &((group)->memory_block) )
#define get_memory_group_entry( T, group, index ) \
	(get_memory_group_group( T, group )[index])

#define new_memory_group_total( T, group, total ) \
	new_memory_block( &((group)->memory_block), sizeof(T) * total )
#define alt_memory_group_total( T, group, total ) \
	alt_memory_block( &((group)->memory_block), sizeof(T) * total )	
#define inc_memory_group_total( T, group, total ) \
	inc_memory_block( &((group)->memory_block), sizeof(T) * total )
#define dec_memory_group_total( T, group, total ) \
	dec_memory_block( &((group)->memory_block), sizeof(T) * total )
#define del_memory_group_total( group ) \
	del_memory_block( &((group)->memory_block) )

#endif
