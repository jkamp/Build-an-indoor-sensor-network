#include "emergency_net/neighbors.h"

#include "dev/cc2420.h"

#include "stddef.h" /* NULL */

#include "base/log.h"

void neighbors_init(struct neighbors *ns) {
	QUEUE_BUFFER_INIT_WITH_STRUCT(ns, nbuf, sizeof(struct neighbor_node), MAX_NEIGHBORS);
}

static int neighbor_to_addr_cmp(const void *lhs, const void *rhs) {
	struct neighbor_node *l = (struct neighbor_node*) lhs;
	rimeaddr_t *r = (rimeaddr_t*) rhs;
	return rimeaddr_cmp(&l->addr, r);
}

void neighbors_add(struct neighbors *ns, const rimeaddr_t *addr) {
	struct neighbor_node nn;
	struct neighbor_node *ret;
	rimeaddr_copy(&nn.addr, addr);
	nn.warnings = 0;
	//nn.time_diff = 0;
	ret = (struct neighbor_node*)queue_buffer_push_front(&ns->nbuf, &nn);
	ASSERT(ret != NULL);
}
void neighbors_remove(struct neighbors *ns, const rimeaddr_t *addr) {
	struct neighbor_node *nn = (struct neighbor_node*)
		queue_buffer_find(&ns->nbuf, addr, neighbor_to_addr_cmp);
	if (nn != NULL) {
		queue_buffer_free(&ns->nbuf, nn);
	}
}


//rtimer_clock_t neighbors_update_time_diff(struct neighbors *ns, const rimeaddr_t *addr) {
//	struct neighbor_node *nn = (struct neighbor_node*)
//		queue_buffer_find(&ns->nbuf, addr, neighbor_to_addr_cmp);
//	ASSERT(nn != NULL);
//	nn->time_diff = cc2420_time_of_arrival - cc2420_time_of_departure;
//
//	LOG("Timediff: %d\n", nn->time_diff);
//
//	return nn->time_diff;
//}

int8_t neighbors_is_neighbor(struct neighbors *ns, const rimeaddr_t *addr) {
	return queue_buffer_find(&ns->nbuf, addr, neighbor_to_addr_cmp) != NULL;
}

void neighbors_warn(struct neighbors *ns, const rimeaddr_t *addr) {
	struct neighbor_node *nn = (struct neighbor_node*)
		queue_buffer_find(&ns->nbuf, addr, neighbor_to_addr_cmp);
	ASSERT(nn != NULL);
	++nn->warnings;
	LOG("Neighbor %d.%d warnings: %d\n", nn->addr.u8[0], nn->addr.u8[1], nn->warnings);
}
