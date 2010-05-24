#include "emergency_net/packet_buffer.h"

#include "string.h"

#include "base/log.h"

static
void add_buffered_packet_tail(struct buffered_packet **head,
		struct buffered_packet *p) {
	p->next = NULL;
	if(*head != NULL) {
		struct buffered_packet *iter = *head;
		while (iter->next != NULL)
			iter = iter->next;
		iter->next = p;
	} else {
		*head = p;
	}
}

static struct buffered_packet* 
allocate_buffered_packet(struct packet_buffer *pb, int prio) {
	struct buffered_packet *s = (struct
			buffered_packet*)queue_buffer_alloc_front(pb->buffer);
	add_buffered_packet_tail(&pb->prio_heads[prio], s);
	return s;
}

void packet_buffer_init(struct packet_buffer *pb) {
	int i;
	for (i = 0; i < PACKET_BUFFER_MAX_TYPES; ++i) {
		pb->prio_heads[i] = NULL;
	}
}

static inline 
void copy_neighbors(struct buffered_packet *bp, const struct neighbors *ns) {
	const struct neighbor_node *nn = neighbors_begin(ns);
	for(; nn != NULL; nn = neighbors_next(ns)) {
		queue_buffer_push_front(&bp->unacked_ns, neighbor_node_addr(nn));
	}
}


struct buffered_packet*
packet_buffer_packet(struct packet_buffer *pb, const struct packet *p, 
		const void *data, uint8_t data_len, const struct neighbors *ns, 
		int prio) {
	struct buffered_packet *s = allocate_buffered_packet(pb, prio);

	if (s != NULL) {
		if (ns != NULL) {
			QUEUE_BUFFER_INIT_WITH_STRUCT(s, unacked_ns, sizeof(rimeaddr_t*), neighbors_size(ns));
			copy_neighbors(s, ns);
		}
		s->times_sent = 0;
		s->data_len = data_len;
		ASSERT(PACKET_HDR_SIZE+data_len < MAX_PACKET_SIZE);
		memcpy(&s->p, p, PACKET_HDR_SIZE);
		memcpy(s->p.data, data, data_len);
		return s;
	}

	LOG("WARNING: Queue FULL. Dropping packet\n");
	return NULL;
}

struct buffered_packet*
packet_buffer_broadcast_packet(struct packet_buffer *pb, 
		const struct broadcast_packet *bp, const void *data, uint8_t data_len,
		const struct neighbors *ns, int prio) {
	struct buffered_packet *s = allocate_buffered_packet(pb, prio);

	if (s != NULL) {
		struct broadcast_packet *tmp = (struct broadcast_packet*)&s->p;
		if (ns != NULL) {
			QUEUE_BUFFER_INIT_WITH_STRUCT(s, unacked_ns, sizeof(rimeaddr_t*), neighbors_size(ns));
			copy_neighbors(s, ns);
		}
		s->times_sent = 0;
		s->data_len = data_len;
		ASSERT(BROADCAST_PACKET_HDR_SIZE+data_len < MAX_PACKET_SIZE);
		memcpy(tmp, bp, BROADCAST_PACKET_HDR_SIZE);
		memcpy(tmp->data, data, data_len);
		return s;
	} 

	LOG("WARNING: Queue FULL. Dropping packet\n");
	return NULL;
}

struct buffered_packet*
packet_buffer_unicast_packet(struct packet_buffer *pb, 
		const struct unicast_packet *up, const void *data, uint8_t data_len,
		const struct neighbors *ns,int prio) {
	struct buffered_packet *s = allocate_buffered_packet(pb, prio);

	if (s != NULL) {
		struct unicast_packet *tmp = (struct unicast_packet*)&s->p;
		if (ns != NULL) {
			QUEUE_BUFFER_INIT_WITH_STRUCT(s, unacked_ns, sizeof(rimeaddr_t*), neighbors_size(ns));
			copy_neighbors(s, ns);
		}
		s->times_sent = 0;
		s->data_len = data_len;
		ASSERT(UNICAST_PACKET_HDR_SIZE+data_len < MAX_PACKET_SIZE);
		memcpy(tmp, up, UNICAST_PACKET_HDR_SIZE);
		memcpy(tmp->data, data, data_len);
		return s;
	} 

	LOG("WARNING: Queue FULL. Dropping packet\n");
	return NULL;
}

/*struct buffered_packet*
packet_buffer_get_packet_for_sending(struct packet_buffer *pb) {
	int i;
	for(i = 0; i < NUM_PRIO_LEVELS; ++i) {
		if (pb->prio_heads[i] != NULL) {
			return pb->prio_heads[i];
		}
	}

	return NULL;
}*/

struct buffered_packet*
packet_buffer_get_first_packet_from_type(struct packet_buffer *pb, uint8_t type) {
	if (pb->prio_heads[type] != NULL) {
		return pb->prio_heads[type];
	}

	return NULL;
}

struct buffered_packet*
packet_buffer_find_buffered_packet(struct packet_buffer *pb, const struct packet* p,
		int (*comparer)(const void *buffered_item, const void *supplied_item)) {

	int i;
	struct buffered_packet *bp;
	for(i = 0; i < PACKET_BUFFER_MAX_TYPES; ++i) {
		for(bp = pb->prio_heads[i]; bp != NULL; bp = bp->next) {
			if (comparer(&bp->p, p)) {
				return bp;
			}
		}
	}

	return NULL;
}

void packet_buffer_clear_priority(struct packet_buffer *pb, int prio) {
	struct buffered_packet *i = pb->prio_heads[prio];
	struct buffered_packet *next;
	for(;i != NULL; i = next) {
		next = i->next;
		queue_buffer_free(pb->buffer, i);
	}

	pb->prio_heads[prio] = NULL;
}

void packet_buffer_free(struct packet_buffer *pb, struct buffered_packet *bp) {
	struct buffered_packet *s;
	struct buffered_packet *s_prev;
	int i;

	for(i = 0; i < PACKET_BUFFER_MAX_TYPES; ++i) {
		s = pb->prio_heads[i];
		s_prev = NULL;
		while (s != NULL) {
			if (s == bp) {
				if (s_prev == NULL)
					pb->prio_heads[i] = s->next;
				else
					s_prev->next = s->next;

				queue_buffer_free(pb->buffer, s);
				return;
			} 

			s_prev = s;
			s = s->next;
		}
	}

	/* Should never happen. */
	ASSERT(0);
}
