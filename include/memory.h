#ifndef MEMORY_H
#define MEMORY_H

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <limits.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <malloc.h>
#include <string.h>
#endif

#ifdef __cplusplus
#define MEMORY_INLINE inline
#elif defined( STDC_VERSION ) && STDC_VERSION >= 199900L
#define MEMORY_INLINE inline
#elif !defined( STDC_VERSION ) && defined( __GNU__ )
#define MEMORY_INLINE inline
#else
#define MEMORY_INLINE static
#endif

#ifndef bitsof
#define bitsof(T) (sizeof(T) * CHAR_BIT)
#endif

typedef void* (*cbAlloc)( void *ud, void *prev, size_t size, size_t want );

static void* default_alloc( void *ud, void *prev, size_t size, size_t want )
{
	if ( want )
	{
		prev = prev ? realloc( prev, want ) : malloc( want );
		
		if ( prev && want > size )
			memset( prev + size, 0, want - size );
		
		return prev;
	}
	
	if ( prev ) free( prev );
	return NULL;
}

static cbAlloc memory = default_alloc;

struct memory_block
{
	void *block;
	size_t bytes;
};

MEMORY_INLINE size_t get_memory_block_bytes( struct memory_block *memory_block )
{
	return memory_block->bytes;
}
MEMORY_INLINE void* get_memory_block_block( struct memory_block *memory_block )
{
	return memory_block->block;
}

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
MEMORY_INLINE void* new_memory_block
(
	void *ud
	, struct memory_block *memory_block
	, size_t bytes
)
{
	void *tmp = memory( ud, NULL, memory_block->bytes, bytes );
	if ( tmp )
	{
		memory_block->block = tmp;
		memory_block->bytes = bytes;
	}
	return tmp;
}

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
MEMORY_INLINE void* alt_memory_block
(
	void *ud
	, struct memory_block *memory_block
	, size_t bytes
)
{
	void *tmp = memory( ud, memory_block->block, memory_block->bytes, bytes );
	if ( tmp )
	{
		memory_block->block = tmp;
		memory_block->bytes = bytes;
	}
	return tmp;
}

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
MEMORY_INLINE void* inc_memory_block
(
	void *ud
	, struct memory_block *memory_block
	, size_t bytes
)
{
	if ( bytes > memory_block->bytes )
	{
		void *tmp = memory( ud, memory_block->block, memory_block->bytes, bytes );
		if ( tmp )
		{
			memory_block->block = tmp;
			memory_block->bytes = bytes;
		}
		return tmp;
	}
	return memory_block->block;
}

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
MEMORY_INLINE void* dec_memory_block
(
	void *ud
	, struct memory_block *memory_block
	, size_t bytes
)
{
	if ( bytes < memory_block->bytes )
	{
		void *tmp = memory( ud, memory_block->block, memory_block->bytes, bytes );
		if ( tmp )
		{
			memory_block->block = tmp;
			memory_block->bytes = bytes;
		}
		return tmp;
	}
	return memory_block->block;
}

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
MEMORY_INLINE void del_memory_block
(
	void *ud
	, struct memory_block *memory_block
)
{
	(void)memory( ud, memory_block->block, memory_block->bytes, 0 );
	memory_block->block = NULL;
	memory_block->bytes = 0;
}

struct memory_group
{
	struct memory_block memory_block;
	int total, count, focus;
	size_t Tsize;
};

static int get_memory_group_focus( struct memory_group *memory_group )
{
	return memory_group->focus;
}
static int get_memory_group_count( struct memory_group *memory_group )
{
	return memory_group->count;
}
static int get_memory_group_total( struct memory_group *memory_group )
{
	return memory_group->total;
}
static size_t get_memory_group_bytes( struct memory_group *memory_group )
{
	return get_memory_block_bytes( &(memory_group->memory_block) );
}
static void* get_memory_group_block( struct memory_group *memory_group )
{
	return get_memory_block_block( &(memory_group->memory_block) );
}
#define get_memory_group_entry( T, memory_group, index ) \
	(((T*)get_memory_group_block(memory_group))[index])

/* It was un-intended but look at what the abbreviations of Index, Count &
 * Total spell out :D */
static void set_memory_group_focus( struct memory_group *memory_group, int I )
{
	I = (I >= 0 && I < memory_group->total) * I;
	memory_group->focus = I;
}
static void set_memory_group_count
(
	struct memory_group *memory_group, int count
)
{
	count = (count >= 0 && count < memory_group->total) * count; 
	memory_group->count = count;
}

MEMORY_INLINE void* new_memory_group_block
(
	void *ud
	, struct memory_group *memory_group
	, int total
)
{
	void *ptr;
	total = (total >= 0) * total;
	ptr = new_memory_block(
		ud, &(memory_group->memory_block), total * memory_group->Tsize );
	memory_group->total =
		memory_group->memory_block.bytes / memory_group->Tsize;
	return ptr;
}

MEMORY_INLINE void* alt_memory_group_total
(
	void *ud
	,struct memory_group *memory_group
	, int total
)
{
	void *ptr;
	total = (total >= 0) * total;
	ptr = alt_memory_block(
		ud, &(memory_group->memory_block), total * memory_group->Tsize );
	memory_group->total =
		memory_group->memory_block.bytes / memory_group->Tsize;
	return ptr;
}

MEMORY_INLINE void* inc_memory_group_total
(
	void *ud
	, struct memory_group *memory_group
	, int total
)
{
	void *ptr;
	total = memory_group->total + ((total >= 0) * total);
	total = ((total >= memory_group->total) * total)
		+ ((total < memory_group->total) * memory_group->total);
	ptr = inc_memory_block(
		ud, &(memory_group->memory_block), total * memory_group->Tsize );
	memory_group->total =
		memory_group->memory_block.bytes / memory_group->Tsize;
	return ptr;
}

MEMORY_INLINE void* dec_memory_group_total
(
	void *ud
	, struct memory_group *memory_group
	, int total
)
{
	void *ptr;
	total = memory_group->total + ((total >= 0) * total);
	total = (total >= 0) * total;
	ptr = dec_memory_block(
		ud, &(memory_group->memory_block), total * memory_group->Tsize );
	memory_group->total =
		memory_group->memory_block.bytes / memory_group->Tsize;
	return ptr;
}

MEMORY_INLINE void del_memory_group_block
(
	void* ud
	, struct memory_group *memory_group
)
{
	del_memory_block( ud, &(memory_group->memory_block) );
	memset( memory_group, 0, sizeof(struct memory_group) );
}

#endif
