#ifndef _PACKET_BUFFER_H_
#define _PACKET_BUFFER_H_

#include "emergency_net/packet.h"
#include "emergency_net/neighbors.h"

#include "base/queue_buffer.h"

/* XXX: change name to packet_send_buffer or something */

#define PACKET_BUFFER_TYPE_ZERO 0
#define PACKET_BUFFER_MAX_TYPES 7

#define BUFFERED_PACKET_HDR_SIZE (sizeof(struct buffered_packet)-sizeof(uint8_t))

#define PACKET_BUFFER(name, num_packets) \
	QUEUE_BUFFER(name##_qbuffer, BUFFERED_PACKET_HDR_SIZE+MAX_PACKET_SIZE, num_packets); \
	struct packet_buffer name

#define PACKET_BUFFER_INIT_WITH_STRUCT(s, name, num_packets) \
	QUEUE_BUFFER_INIT_WITH_STRUCT((s), name##_qbuffer, BUFFERED_PACKET_HDR_SIZE+MAX_PACKET_SIZE, num_packets); \
	(s)->name.buffer = &(s)->name##_qbuffer; \
	packet_buffer_init(&(s)->name)
	
struct buffered_packet {
	struct buffered_packet *next;
	/* Neighbors who are still to ack the packet */
	QUEUE_BUFFER(unacked_ns, sizeof(rimeaddr_t), MAX_NEIGHBORS);
	uint8_t times_sent; 
	uint8_t data_len; 
	/*void (*send_fn)(void *ptr);*/
	struct packet p;
};

struct packet_buffer {
	struct buffered_packet *prio_heads[PACKET_BUFFER_MAX_TYPES];
	struct queue_buffer *buffer;
};

void packet_buffer_init(struct packet_buffer *pb);

struct buffered_packet*
packet_buffer_packet(struct packet_buffer *pb, const struct packet *p, 
		const void *data, uint8_t data_len, const struct neighbors *ns
		/*void (*send_fn)(void *ptr)*/, int type);

struct buffered_packet*
packet_buffer_broadcast_packet(struct packet_buffer *pb, 
		const struct broadcast_packet *bp, const void *data, uint8_t data_len,
		const struct neighbors *ns/*, void (*send_fn)(void *ptr)*/, int type);

struct buffered_packet*
packet_buffer_unicast_packet(struct packet_buffer *pb, 
		const struct unicast_packet *up, const void *data, uint8_t data_len,
		const struct neighbors *ns/*, void (*send_fn)(void *ptr)*/, int type);

static
void packet_buffer_neighbor_acked(struct buffered_packet *bp, const rimeaddr_t *addr);

static
int packet_buffer_num_unacked_neighbors(struct buffered_packet *bp);

static
int packet_buffer_all_neighbors_acked(const struct buffered_packet *bp);

static
rimeaddr_t* packet_buffer_unacked_neighbors_begin(struct buffered_packet *bp);

static
rimeaddr_t* packet_buffer_unacked_neighbors_next(struct buffered_packet *bp);

static
uint8_t packet_buffer_times_sent(const struct buffered_packet *bp);

static
uint8_t packet_buffer_data_len(const struct buffered_packet *bp);

static
struct packet* packet_buffer_get_packet(struct buffered_packet *bp);

/*static
void* packet_buffer_get_data(struct buffered_packet *bp);*/

static
void packet_buffer_increment_times_sent(struct buffered_packet *bp);

/*static
void (*packet_buffer_send_fn(struct buffered_packet *bp)) (void*);*/

/*struct buffered_packet*
packet_buffer_get_packet_for_sending(struct packet_buffer *pb);*/
struct buffered_packet*
packet_buffer_get_first_packet_from_type(struct packet_buffer *pb, uint8_t type);

struct buffered_packet*
packet_buffer_find_buffered_packet(struct packet_buffer *pb, const struct packet* p,
		int (*comparer)(const void *buffered_item, const void *supplied_item));

static
int packet_buffer_has_room_for_packets(const struct packet_buffer *pb, 
		uint8_t num_packets);

void packet_buffer_clear_priority(struct packet_buffer *pb, int prio);

void packet_buffer_free(struct packet_buffer *pb, struct buffered_packet *bp);


/************************* Inline Definitions **************************/

static inline
uint8_t packet_buffer_times_sent(const struct buffered_packet *bp) {
	return bp->times_sent;
}

static inline
uint8_t packet_buffer_data_len(const struct buffered_packet *bp) {
	return bp->data_len;
}

static inline
struct packet* packet_buffer_get_packet(struct buffered_packet *bp) {
	return &bp->p;
}

/*static inline
void* packet_buffer_get_data(struct buffered_packet *bp) {
	return bp->data;
}*/

static inline
void packet_buffer_increment_times_sent(struct buffered_packet *bp) {
	++bp->times_sent;
}

/*static inline
void (*packet_buffer_send_fn(struct buffered_packet *bp)) (void*) {
	return bp->send_fn;
}*/

static inline
int packet_buffer_has_room_for_packets(const struct packet_buffer *pb, uint8_t num_packets) {
	return queue_buffer_size(pb->buffer)+num_packets <=
		queue_buffer_max_size(pb->buffer);
}

static inline
void packet_buffer_neighbor_acked(struct buffered_packet *bp, const rimeaddr_t *addr) {
	rimeaddr_t *baddr = (rimeaddr_t*)queue_buffer_begin(&bp->unacked_ns);
	for(; baddr != NULL; baddr = (rimeaddr_t*)queue_buffer_next(&bp->unacked_ns)) {
		if (rimeaddr_cmp(baddr, addr)) {
			queue_buffer_free(&bp->unacked_ns, baddr);
			return;
		}
	}
}

static inline
int packet_buffer_all_neighbors_acked(const struct buffered_packet *bp) {
	return queue_buffer_size(&bp->unacked_ns) == 0;
}

static inline
int packet_buffer_num_unacked_neighbors(struct buffered_packet *bp) {
	return queue_buffer_size(&bp->unacked_ns);
}

static inline
rimeaddr_t* packet_buffer_unacked_neighbors_begin(struct buffered_packet *bp) {
	return (rimeaddr_t*)queue_buffer_begin(&bp->unacked_ns);
}

static inline
rimeaddr_t* packet_buffer_unacked_neighbors_next(struct buffered_packet *bp) {
	return (rimeaddr_t*)queue_buffer_next(&bp->unacked_ns);
}
#endif
