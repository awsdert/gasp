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
#include <string.h>
#endif

struct memory_block
{
	void *block;
	size_t bytes;
};

static size_t get_memory_block_bytes( struct memory_block *memory_block )
{
	return memory_block->bytes;
}
static void* get_memory_block_block( struct memory_block *memory_block )
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
static void set_memory_group_count( struct memory_group *memory_group, int C )
{
	C = (C >= 0 && C < memory_group->total) * C; 
	memory_group->count = C;
}

static void* new_memory_group_block( struct memory_group *memory_group, int T )
{
	void *ptr;
	T = (T >= 0) * T;
	ptr = new_memory_block(
		&(memory_group->memory_block), T * memory_group->Tsize );
	memory_group->total =
		memory_group->memory_block.bytes / memory_group->Tsize;
	return ptr;
}

static void* alt_memory_group_total( struct memory_group *memory_group, int T )
{
	void *ptr;
	T = (T >= 0) * T;
	ptr = alt_memory_block(
		&(memory_group->memory_block), T * memory_group->Tsize );
	memory_group->total =
		memory_group->memory_block.bytes / memory_group->Tsize;
	return ptr;
}

static void* inc_memory_group_total( struct memory_group *memory_group, int T )
{
	void *ptr;
	T = memory_group->total + ((T >= 0) * T);
	T = ((T >= memory_group->total) * T)
		+ ((T < memory_group->total) * memory_group->total);
	ptr = inc_memory_block(
		&(memory_group->memory_block), T * memory_group->Tsize );
	memory_group->total =
		memory_group->memory_block.bytes / memory_group->Tsize;
	return ptr;
}

static void* dec_memory_group_total( struct memory_group *memory_group, int T )
{
	void *ptr;
	T = memory_group->total + ((T >= 0) * T);
	T = (T >= 0) * T;
	ptr = dec_memory_block(
		&(memory_group->memory_block), T * memory_group->Tsize );
	memory_group->total =
		memory_group->memory_block.bytes / memory_group->Tsize;
	return ptr;
}

static void del_memory_group_block( struct memory_group *memory_group )
{
	del_memory_block( &(memory_group->memory_block) );
	memset( memory_group, 0, sizeof(struct memory_group) );
}
	

#endif
