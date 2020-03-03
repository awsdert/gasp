#include "gasp.h"
void* change_nodes(
	int *err, nodes_t *nodes, size_t Nsize, node_t want, int dir ) {
	errno = EXIT_SUCCESS;
	if ( !nodes ) {
		if ( err ) *err = EDESTADDRREQ;
		return NULL;
	}
	if ( !Nsize ) {
		if ( !want ) {
			free_space( err, &(nodes->space) );
			nodes->total = nodes->count = 0;
			return NULL;
		}
		if ( err ) *err = EINVAL;
		return NULL;
	}
	if ( !change_space(err, &(nodes->space), want * Nsize, dir) )
		return NULL;
	nodes->total = nodes->space.given / Nsize;
	return nodes->space.block;
}

int add_node( nodes_t *nodes, node_t *node, size_t Nsize ) {
	int ret = EXIT_SUCCESS;
	if ( !nodes || !node ) return EDESTADDRREQ;
	if ( nodes->count >= nodes->total ) {
		if ( !change_nodes( &ret, nodes, Nsize, nodes->count + 100, 1 ) )
			return ret;
	}
	*node = nodes->count;
	nodes->count++;
	return ret;
}
