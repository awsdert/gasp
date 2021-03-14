#include <memory.h>

void * new_memory_block( struct memory_block *memory_block, size_t bytes )
{
	memory_block->block = malloc( bytes );
	memory_block->bytes = !!(memory_block) * bytes;
	return memory_block->block;
}

void * alt_memory_block( struct memory_block *memory_block, size_t bytes )
{
	void * block = realloc( memory_block->block, bytes );
	uintptr_t old = (uintptr_t)(memory_block->block), nxt = (uintptr_t)block;
	memory_block->block = (void*)(nxt + (!nxt * old));
	memory_block->bytes = (!!nxt * bytes) + (!nxt * memory_block->bytes);
	return block;
}

void * inc_memory_block( struct memory_block *memory_block, size_t bytes )
{
	if ( bytes > memory_block->bytes )
	{
		void * block = realloc( memory_block->block, bytes );
		uintptr_t old = (uintptr_t)(memory_block->block), nxt = (uintptr_t)block;
		memory_block->block = (void*)(nxt + (!nxt * old));
		memory_block->bytes = (!!nxt * bytes) + (!nxt * memory_block->bytes);
		return block;
	}
	return NULL;
}

void * dec_memory_block( struct memory_block *memory_block, size_t bytes )
{
	if ( bytes > memory_block->bytes )
	{
		void * block = realloc( memory_block->block, bytes );
		uintptr_t old = (uintptr_t)(memory_block->block), nxt = (uintptr_t)block;
		memory_block->block = (void*)(nxt + (!nxt * old));
		memory_block->bytes = (!!nxt * bytes) + (!nxt * memory_block->bytes);
		return block;
	}
	return NULL;
}

void del_memory_block( struct memory_block *memory_block )
{
	free( memory_block->block );
	memory_block->block = NULL;
	memory_block->bytes = 0;
}
