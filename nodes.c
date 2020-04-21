#include "gasp.h"
int change_nodes(
	nodes_t *nodes, size_t Nsize, node_t want, int dir
)
{
	int ret = 0;
	char *pof;
	
	pof = "destination check";
	if ( !nodes )
	{
		ret = EDESTADDRREQ;
		goto fail;
	}
	
	pof = "node size check";
	if ( !Nsize ) {
		if ( !want ) {
			free_space( &(nodes->space) );
			nodes->total = nodes->count = nodes->focus = 0;
			return 0;
		}
		ret = EINVAL;
		goto fail;
	}
	
	pof = "memory allocation";
	if ( (ret = change_space(
		&(nodes->space), want * Nsize, dir)) != 0
	) goto fail;
	
	nodes->total = nodes->space.given / Nsize;
	if ( nodes->focus >= nodes->total )
		nodes->focus = 0;
	
	if ( ret != 0 )
	{
		fail:
		FAILED( ret, pof );
	}
	
	return ret;
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
