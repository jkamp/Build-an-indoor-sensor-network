#ifndef _NEIGHBORS_H_
#define _NEIGHBORS_H_

#include "sys/rtimer.h"

#include "net/rime/rimeaddr.h"
#include "base/queue_buffer.h"

#define MAX_NEIGHBORS 8

struct neighbor_node {
	rimeaddr_t addr;
	uint8_t warnings;
	//int8_t rssi;
	//rtimer_clock_t time_diff;
};

struct neighbors {
	//int8_t rssi_max;
	QUEUE_BUFFER(nbuf, sizeof(struct neighbor_node), MAX_NEIGHBORS);
};

void neighbors_init(struct neighbors *ns);


void neighbors_add(struct neighbors *ns, const rimeaddr_t *addr);
void neighbors_remove(struct neighbors *ns, const rimeaddr_t *addr);

//rtimer_clock_t 
//neighbors_update_time_diff(struct neighbors *ns, const rimeaddr_t *addr);

int8_t neighbors_is_neighbor(struct neighbors *ns, const rimeaddr_t *addr);

static inline
uint8_t neighbors_size(struct neighbors *ns); 

static inline
void neighbors_copy(struct neighbors *to, const struct neighbors *from);

static inline 
struct neighbor_node* 
neighbors_begin(struct neighbors *ns);

static inline 
struct neighbor_node* 
neighbors_next(struct neighbors *ns);

void neighbors_warn(struct neighbors *ns, const rimeaddr_t *addr);

/************************* Inline Definitions **************************/

uint8_t neighbors_size(struct neighbors *ns) {
	return queue_buffer_size(&ns->nbuf);
}

void neighbors_copy(struct neighbors *to, const struct neighbors *from) {
	queue_buffer_copy(&to->nbuf, &from->nbuf);
}

struct neighbor_node* 
neighbors_begin(struct neighbors *ns) {
	return (struct neighbor_node*)queue_buffer_begin(&ns->nbuf);
}

struct neighbor_node* 
neighbors_next(struct neighbors *ns) {
	return (struct neighbor_node*)queue_buffer_next(&ns->nbuf);
}

#endif
