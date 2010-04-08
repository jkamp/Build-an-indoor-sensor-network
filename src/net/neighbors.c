#include "neighbors.h"

#include "stddef.h" /* NULL */

#include "base/log.h"

void neighbors_init(struct neighbors *ns) {
	ns->rssi_max = -127;
	QUEUE_BUFFER_INIT_WITH_STRUCT(ns, nbuf, sizeof(struct neighbor_node), MAX_NEIGHBORS);
}

static int neighbor_to_addr_cmp(const void *lhs, const void *rhs) {
	struct neighbor_node *l = (struct neighbor_node*) lhs;
	rimeaddr_t *r = (rimeaddr_t*) rhs;
	return rimeaddr_cmp(&l->addr, r);
}

inline
static void update_rssi_max(struct neighbors *ns, int8_t rssi) {
	if (rssi > ns->rssi_max) {
		ns->rssi_max = rssi;
	}
}

struct neighbor_node* 
neighbors_update(struct neighbors* ns, const rimeaddr_t *sender, int8_t rssi) {
	struct neighbor_node *n = queue_buffer_find(&ns->nbuf, sender,
			neighbor_to_addr_cmp);
	LOG("Neighbor update: addr:%d.%d, rssi:%d. rssi_max: %d\n", sender->u8[0],
			sender->u8[1], rssi, ns->rssi_max);
	if (n != NULL) {
		LOG("Found OLD neighbor\n");
		n->rssi = (n->rssi + rssi)/2;
		if (n->rssi < ns->rssi_max * 2) {
			LOG("Removing neighbor\n");
			queue_buffer_free(&ns->nbuf, n);
			return NULL;
		}
		update_rssi_max(ns, n->rssi);
		return n;
	}

	if (rssi >= ns->rssi_max * 2) {
		LOG("Found a NEW neighbor\n");
		n = (struct neighbor_node*)queue_buffer_alloc_front(&ns->nbuf);
		if (n != NULL) {
			rimeaddr_copy(&n->addr, sender);
			n->rssi = rssi;
			update_rssi_max(ns, n->rssi);
			return n; 
		} else {
			int8_t worst_rssi = 127;
			struct neighbor_node *i;
			for (i = (struct neighbor_node*) queue_buffer_begin(&ns->nbuf); 
					i != NULL; 
					i = (struct neighbor_node*)queue_buffer_next(&ns->nbuf)) {
				if (i->rssi < worst_rssi) {
					worst_rssi = i->rssi;
					n = i;
				}
			}
			LOG("Neighbors buffer full, throwing out worst neighbor (delta"
				" RSSI: %i)\n", (rssi-worst_rssi));
			if (rssi > worst_rssi) {
				rimeaddr_copy(&n->addr, sender);
				n->rssi = rssi;
				update_rssi_max(ns, n->rssi);
				return n;
			}
		}
	}

	/* didnt cut it */
	return NULL;
}

enum neighbor_status
neighbors_classify_neighbor(struct neighbors *ns, const rimeaddr_t *naddr) {
	struct neighbor_node *n = (struct neighbor_node*)
		queue_buffer_find(&ns->nbuf, naddr, neighbor_to_addr_cmp);

	if (n != NULL)
		return IS_NEIGHBOR;
	return IS_NOT_NEIGHBOR;
}

