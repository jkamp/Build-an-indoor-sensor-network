#ifndef _NEIGHBORS_H_
#define _NEIGHBORS_H_

#include "sys/rtimer.h"
#include "net/rime/rimeaddr.h"

#include "emergency_net/neighbor_node.h"

#include "base/queue_buffer.h"

#define MAX_NEIGHBORS 8

struct neighbors {
	QUEUE_BUFFER(nbuf, sizeof(struct neighbor_node), MAX_NEIGHBORS);
};

void neighbors_init(struct neighbors *ns);

void neighbors_add(struct neighbors *ns, const rimeaddr_t *addr);
void neighbors_remove(struct neighbors *ns, const rimeaddr_t *addr);

int neighbors_is_neighbor(const struct neighbors *ns, const rimeaddr_t *addr);

static
uint8_t neighbors_size(const struct neighbors *ns); 

static
const struct neighbor_node* neighbors_begin(const struct neighbors *ns);

static
const struct neighbor_node* neighbors_next(const struct neighbors *ns);

struct neighbor_node* 
neighbors_find_neighbor_node(struct neighbors *ns, const rimeaddr_t *addr);

//void neighbors_warn(struct neighbors *ns, const rimeaddr_t *addr);

void neighbors_clear(struct neighbors *ns);

/************************* Inline Definitions **************************/

static inline
uint8_t neighbors_size(const struct neighbors *ns) {
	return queue_buffer_size(&ns->nbuf);
}

static inline
const struct neighbor_node* neighbors_begin(const struct neighbors *ns) {
	return (const struct neighbor_node*)queue_buffer_begin((struct
				queue_buffer*)&ns->nbuf);
}

static inline
const struct neighbor_node* neighbors_next(const struct neighbors *ns) {
	return (const struct neighbor_node*)queue_buffer_next((struct
				queue_buffer*)&ns->nbuf);
}
#endif
