#ifndef _NEIGHBORS_H_
#define _NEIGHBORS_H_

#define MAX_NEIGHBORS 8

#include "net/rime/rimeaddr.h"

#include "base/queue_buffer.h"

enum neighbor_status {
	IS_NEIGHBOR,
	IS_NOT_NEIGHBOR
};

struct neighbor_node {
	rimeaddr_t addr;
	int8_t rssi;
};

struct neighbors {
	int8_t rssi_max;
	QUEUE_BUFFER(nbuf, sizeof(struct neighbor_node), MAX_NEIGHBORS);
};

void neighbors_init(struct neighbors *ns);

struct neighbor_node* 
neighbors_update(struct neighbors* ns, const rimeaddr_t *sender, int8_t rssi);

enum neighbor_status
neighbors_classify_neighbor(struct neighbors *ns, const rimeaddr_t *naddr);

static inline
uint8_t neighbors_size(struct neighbors *ns); 

uint8_t neighbors_size(struct neighbors *ns) {
	return queue_buffer_size(&ns->nbuf);
}
#endif
