#include "packet_buffer.h"

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
allocate_buffered_packet(struct packet_buffer *pb, uint8_t prio) {
	struct buffered_packet *s = (struct
			buffered_packet*)queue_buffer_alloc_front(pb->buffer);
	add_buffered_packet_tail(&pb->prio_heads[prio], s);
	return s;
}

void packet_buffer_init(struct packet_buffer *pb) {
	int i;
	for (i = 0; i < NUM_PRIO_LEVELS; ++i) {
		pb->prio_heads[i] = NULL;
	}
}


struct buffered_packet*
packet_buffer_packet(struct packet_buffer *pb, const struct packet *p, 
		const void *data, uint8_t data_len, uint8_t prio) {
	struct buffered_packet *s = allocate_buffered_packet(pb, prio);

	if (s != NULL) {
		s->times_acked = 0;
		s->times_sent = 0;
		s->data_len = data_len;
		memcpy(&s->p, p, HEADER_SIZE);
		memcpy(s->data, data, data_len);
		return s;
	} 

	LOG("WARNING: Queue FULL. Dropping packet\n");
	return NULL;
}

struct buffered_packet*
packet_buffer_data_packet(struct packet_buffer *pb, const struct data_packet *dp, 
		uint8_t data_len, uint8_t prio) {
	struct buffered_packet *s = allocate_buffered_packet(pb, prio);

	if (s != NULL) {
		s->times_acked = 0;
		s->times_sent = 0;
		s->data_len = data_len;
		memcpy(&s->p, dp, HEADER_SIZE+data_len);
		return s;
	} 

	LOG("WARNING: Queue FULL. Dropping packet\n");
	return NULL;
}

struct buffered_packet*
packet_buffer_ack_packet(struct packet_buffer *pb, const struct ack_packet *ap,
		uint8_t prio) {
	struct buffered_packet *s = allocate_buffered_packet(pb, prio);

	if (s != NULL) {
		s->data_len = ACK_PACKET_SIZE-HEADER_SIZE;
		memcpy(&s->p, ap, ACK_PACKET_SIZE);
		return s;
	} 

	LOG("WARNING: Queue FULL. Dropping packet\n");
	return NULL;
}


struct buffered_packet*
packet_buffer_get_packet_for_sending(struct packet_buffer *pb) {
	int i;
	for(i = 0; i < NUM_PRIO_LEVELS; ++i) {
		if (pb->prio_heads[i] != NULL) {
			return pb->prio_heads[i];
		}
	}

	return NULL;
}

int
packet_buffer_has_packet_for_sending(struct packet_buffer *pb) {

	int i;
	for(i = 0; i < NUM_PRIO_LEVELS; ++i) {
		if (pb->prio_heads[i] != NULL) {
			return 1;
		}
	}

	return 0;
}

static int buffered_packet_to_packet_cmp(const void *buffered, const void *packet) {
	struct buffered_packet *bp = (struct buffered_packet*)buffered;
	return packet_to_packet_cmp(&bp->p, packet);
}

struct buffered_packet* 
packet_buffer_find_packet(struct packet_buffer *pb, const struct packet *p) {
	return (struct buffered_packet*)queue_buffer_find(pb->buffer, p,
			buffered_packet_to_packet_cmp);
}


void packet_buffer_free(struct packet_buffer *pb, struct buffered_packet *bp) {
	struct buffered_packet *s;
	struct buffered_packet *s_prev;
	int i;

	for(i = 0; i < NUM_PRIO_LEVELS; ++i) {
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
}
