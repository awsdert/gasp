#include "gasp.h"
int change_nodes(
	nodes_t *nodes, size_t Nsize, node_t want, int dir
)
{
	int ret = 0;
	
	if ( !nodes ) {
		ret = EDESTADDRREQ;
		ERRMSG( ret, "Need location to fill pointer into" );
		return ret;
	}
	
	if ( !Nsize ) {
		if ( !want ) {
			free_space( &(nodes->space) );
			nodes->total = nodes->count = nodes->focus = 0;
			return 0;
		}
		return EINVAL;
	}
	
	if ( (ret = change_space(
		&(nodes->space), want * Nsize, dir)) != 0
	) return ret;
	
	nodes->total = nodes->space.given / Nsize;
	if ( nodes->focus >= nodes->total )
		nodes->focus = 0;
	return 0;
}

int add_node( nodes_t *nodes, node_t *node, size_t Nsize ) {
	int ret = EXIT_SUCCESS;
	if ( !nodes || !node ) return EDESTADDRREQ;
	if ( nodes->count >= nodes->total ) {
		if ( (ret = change_nodes(
			nodes, Nsize, nodes->count + 100, 1 )) != 0
		) return ret;
	}
	*node = nodes->count;
	nodes->count++;
	return ret;
}
