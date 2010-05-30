#include "emergency_net/neighbors.h"

#include "stddef.h" /* NULL */

#include "base/log.h"

void neighbors_init(struct neighbors *ns) {
	QUEUE_BUFFER_INIT_WITH_STRUCT(ns, nbuf, sizeof(struct neighbor_node), MAX_NEIGHBORS);
	TRACE("initing neighbors\n");
}

static int neighbor_to_addr_cmp(const void *lhs, const void *rhs) {
	struct neighbor_node *l = (struct neighbor_node*) lhs;
	rimeaddr_t *r = (rimeaddr_t*) rhs;
	return rimeaddr_cmp(&l->addr, r);
}

void neighbors_add(struct neighbors *ns, const rimeaddr_t *addr) {
	struct neighbor_node nn;
	memset(&nn, 0, sizeof(struct neighbor_node));
	neighbor_node_set_addr(&nn, addr);
	neighbor_node_set_best_path(&nn, &neighbor_node_best_path_max);

	{
		struct neighbor_node *ret = 
			(struct neighbor_node*)queue_buffer_push_front(&ns->nbuf, &nn);
		ASSERT(ret != NULL);
	}
}

void neighbors_remove(struct neighbors *ns, const rimeaddr_t *addr) {
	struct neighbor_node *nn = (struct neighbor_node*)
		queue_buffer_find(&ns->nbuf, addr, neighbor_to_addr_cmp);
	if (nn != NULL) {
		queue_buffer_free(&ns->nbuf, nn);
	}
}

int neighbors_is_neighbor(const struct neighbors *ns, const rimeaddr_t *addr) {
	return queue_buffer_find((struct queue_buffer*)&ns->nbuf, addr,
			neighbor_to_addr_cmp) != NULL;
}

/*void neighbors_warn(struct neighbors *ns, const rimeaddr_t *addr) {
	struct neighbor_node *nn = (struct neighbor_node*)
		queue_buffer_find(&ns->nbuf, addr, neighbor_to_addr_cmp);
	ASSERT(nn != NULL);
	++nn->warnings;
	LOG("Neighbor %d.%d warnings: %d\n", nn->addr.u8[0], nn->addr.u8[1], nn->warnings);
}*/

struct neighbor_node* 
neighbors_find_neighbor_node(struct neighbors *ns, const rimeaddr_t *addr) {
	return (struct neighbor_node*)queue_buffer_find(&ns->nbuf, addr, neighbor_to_addr_cmp);
}

void neighbors_clear(struct neighbors *ns) {
	queue_buffer_clear(&ns->nbuf);
	ASSERT(queue_buffer_begin(&ns->nbuf) == NULL);
}
