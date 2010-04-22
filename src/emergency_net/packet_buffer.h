#ifndef _PACKET_BUFFER_H_
#define _PACKET_BUFFER_H_

#include "emergency_net/packet.h"
#include "emergency_net/neighbors.h"
#include "base/queue_buffer.h"

#define HIGHEST_PRIO 0
#define NUM_PRIO_LEVELS 2

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
	struct neighbors unacked_ns;
	uint8_t times_sent; 
	uint8_t data_len; 
	struct packet p;
	uint8_t data[1];
};

struct packet_buffer {
	struct buffered_packet *prio_heads[NUM_PRIO_LEVELS];
	struct queue_buffer *buffer;
};

void packet_buffer_init(struct packet_buffer *pb);

struct buffered_packet*
packet_buffer_packet(struct packet_buffer *pb, const struct packet *p, 
		const void *data, uint8_t data_len, const struct neighbors *ns, 
		uint8_t prio);

struct buffered_packet*
packet_buffer_broadcast_packet(struct packet_buffer *pb, 
		const struct broadcast_packet *bp, const void *data, uint8_t data_len,
		const struct neighbors *ns, uint8_t prio);

struct buffered_packet*
packet_buffer_unicast_packet(struct packet_buffer *pb, 
		const struct unicast_packet *up, const void *data, uint8_t data_len,
		const struct neighbors *ns, uint8_t prio);

static inline
struct neighbors* packet_buffer_unacked_neighbors(struct buffered_packet *bp);

static inline
uint8_t packet_buffer_times_sent(const struct buffered_packet *bp);

static inline
uint8_t packet_buffer_data_len(const struct buffered_packet *bp);

static inline
struct packet* packet_buffer_get_packet(struct buffered_packet *bp);

static inline
void* packet_buffer_get_data(struct buffered_packet *bp);

static inline
uint8_t packet_buffer_increment_times_sent(struct buffered_packet *bp);

struct buffered_packet*
packet_buffer_get_packet_for_sending(struct packet_buffer *pb);

int packet_buffer_has_packet_for_sending(struct packet_buffer *pb);

struct buffered_packet*
packet_buffer_find_packet(struct packet_buffer *pb, const struct packet* p,
		int (*comparer)(const void *buffered_item, const void *supplied_item));

static inline
int packet_buffer_has_room_for_packets(const struct packet_buffer *pb, uint8_t
		num_packets);

void packet_buffer_free(struct packet_buffer *pb, struct buffered_packet *bp);


/************************* Inline Definitions **************************/

uint8_t packet_buffer_times_sent(const struct buffered_packet *bp) {
	return bp->times_sent;
}

uint8_t packet_buffer_data_len(const struct buffered_packet *bp) {
	return bp->data_len;
}

struct packet* packet_buffer_get_packet(struct buffered_packet *bp) {
	return &bp->p;
}

void* packet_buffer_get_data(struct buffered_packet *bp) {
	return bp->data;
}

uint8_t packet_buffer_increment_times_sent(struct buffered_packet *bp) {
	return ++bp->times_sent;
}

int packet_buffer_has_room_for_packets(const struct packet_buffer *pb, uint8_t num_packets) {
	return queue_buffer_size(pb->buffer)+num_packets <=
		queue_buffer_max_size(pb->buffer);
}

struct neighbors* packet_buffer_unacked_neighbors(struct buffered_packet *bp) {
	return &bp->unacked_ns;
}
#endif
