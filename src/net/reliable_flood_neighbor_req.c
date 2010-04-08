#include "net/reliable_flood_neighbor_req.h"

#include "string.h"

#include "lib/random.h"

#include "base/log.h"

#define MAX_TIMES_SENT 2
#define SENDING_ACK_INTERVAL_MIN 0
#define SENDING_ACK_INTERVAL_MAX 50
#define SENDING_DATA_INTERVAL_MIN 300
#define SENDING_DATA_INTERVAL_MAX 500

/* TODO(johan): implement max hops so that we avoid routing loops. */
#define MAX_HOPS 100

enum {
	MSG_PRIO_ACK = HIGHEST_PRIO, /* highest prio */
	MSG_PRIO_USER_MSG = HIGHEST_PRIO + 1,
	MSG_PRIO_FORWARD_MSG = HIGHEST_PRIO + 2 /* lowest prio */
};

static void send_ack(void *ptr);
static void send_data(void *ptr);

static void set_timer_for_next_packet(struct rfnr_conn *c) {
	struct buffered_packet *bp =
		packet_buffer_get_packet_for_sending(&c->unacked);

	if (bp != NULL) {
		const struct packet* p = packet_buffer_get_packet(bp);
		c->next_to_send = bp;
		LOG("Next packet to send: ");
		DEBUG_PACKET(p);

		if (IS_ACK(p)) {
			ctimer_set(&c->send_timer, SENDING_ACK_INTERVAL_MIN + 
					random_rand()%
					(SENDING_ACK_INTERVAL_MAX-SENDING_ACK_INTERVAL_MIN),
					send_ack, c);
		} else {
			/* Data packet */
			while (packet_buffer_times_sent(bp) >= MAX_TIMES_SENT) {
				LOG("Packet has been sent too many times without ACKs, neighbor "
						"presumed dead. Dropping packet from further sending.\n");
				packet_buffer_free(&c->unacked, bp);
				bp = packet_buffer_get_packet_for_sending(&c->unacked);
				if (bp == NULL)
					return;
			}
			ctimer_set(&c->send_timer, SENDING_DATA_INTERVAL_MIN + 
					random_rand()%
					(SENDING_DATA_INTERVAL_MAX-SENDING_DATA_INTERVAL_MIN),
					send_data, c);
		}
	} else {
		ctimer_stop(&c->send_timer);
	}
}

static void send_ack(void *ptr) {
	struct rfnr_conn *c = (struct rfnr_conn*)ptr;
	struct packet *p = packet_buffer_get_packet(c->next_to_send);
	uint8_t len = HEADER_SIZE + packet_buffer_data_len(c->next_to_send);

	packetbuf_set_datalen(len);
	memcpy(packetbuf_dataptr(), p, len);

	LOG("[PCKT SEND] type:A, hops:%d, s:%d.%d, o:%d.%d, seqno:%d\n",
			NUM_HOPS(p), p->hdr.sender.u8[0], p->hdr.sender.u8[1],
			p->hdr.originator.u8[0], p->hdr.originator.u8[1],
			p->hdr.originator_seqno);
	if (abc_send(&c->ack_conn) == 0) {
		LOG("ERROR: ACK packet could not be sent.\n");
		set_timer_for_next_packet(c);
		return;
	}

	packet_buffer_free(&c->unacked, c->next_to_send);

	set_timer_for_next_packet(c);
}

static void send_data(void *ptr) {
	struct rfnr_conn *c = (struct rfnr_conn*)ptr;
	struct packet *p = packet_buffer_get_packet(c->next_to_send);
	uint8_t len = HEADER_SIZE + packet_buffer_data_len(c->next_to_send);
	//ASSERT(packet_buffer_find_packet(&c->unacked, p) == c->next_to_send);

	packetbuf_set_datalen(len);
	memcpy(packetbuf_dataptr(), p, len);

	LOG("[PCKT SEND] type:D, hops:%d, s:%d.%d, o:%d.%d, seqno:%d\n",
			NUM_HOPS(p), p->hdr.sender.u8[0], p->hdr.sender.u8[1],
			p->hdr.originator.u8[0], p->hdr.originator.u8[1],
			p->hdr.originator_seqno);
	if (abc_send(&c->data_conn) == 0) {
		LOG("ERROR: Data Packet could not be sent.\n");
		set_timer_for_next_packet(c);
		return;
	}

	packet_buffer_increment_times_sent(c->next_to_send);

	set_timer_for_next_packet(c);
}

static void abc_recv(struct abc_conn *bc) {
	struct rfnr_conn *c;
	const struct packet *p = (struct packet*)packetbuf_dataptr();

	int is_ack = IS_ACK(p);
	if (is_ack) {
		 /* rfnr_conn.ack_conn is one down, correct it. */
		 c = (struct rfnr_conn*)(bc-1);
	} else {
		 c = (struct rfnr_conn*)bc;
	}

	struct neighbor_node *n = neighbors_update(&c->ns, &p->hdr.sender,
			packetbuf_attr(PACKETBUF_ATTR_RSSI));

	if (is_ack) {
		/* Aways accept ACKs, no neighbor requirements here. */
		const struct ack_packet *ap = (struct ack_packet*)p;

		LOG("[PCKT RCV] type:A, hops:%d, s:%d.%d, o:%d.%d, seqno:%d\n",
			NUM_HOPS(p), p->hdr.sender.u8[0], p->hdr.sender.u8[1],
			p->hdr.originator.u8[0], p->hdr.originator.u8[1],
			p->hdr.originator_seqno);

		if (rimeaddr_cmp(&ap->destination, &rimeaddr_node_addr)) {
			/* ack for one of our packets
			   find buffered packet */
			struct buffered_packet *bp =
				packet_buffer_find_packet(&c->unacked, p);
			if (bp != NULL) {
				if (packet_buffer_increment_times_acked(bp) >=
						neighbors_size(&c->ns)) {
					/* every neighbor acked our packet. */
					LOG("Every neighbor acked packet.\n");
					packet_buffer_free(&c->unacked, bp);
					if (c->next_to_send == bp) {
						/* Next to send was our fully acked packet, schedule
						 * new packet.. */
						set_timer_for_next_packet(c);
					}
				}
			} else {
				LOG("Didnt find packet for ACK\n");
			}
		}

		return;
	} else if(n != NULL) {
		/* DATA PACKET */
		/* Passed the neighbor req. */
		const struct data_packet *dp = (struct data_packet*)p;
		struct slim_packet *sp;
		uint8_t data_len;
		uint8_t hops;
		rfnr_instructions_t instructions;

		LOG("[PCKT RCV] type:D, hops:%d, s:%d.%d, o:%d.%d, seqno:%d\n",
				NUM_HOPS(p), p->hdr.sender.u8[0], p->hdr.sender.u8[1],
				p->hdr.originator.u8[0], p->hdr.originator.u8[1],
				p->hdr.originator_seqno);

		if(!rimeaddr_cmp(&p->hdr.originator, &rimeaddr_node_addr)) {

			/* check if we have atleast room for two packets (one ACK, one
			 * forward request from user) */
			if (!packet_buffer_has_room_for_packets(&c->unacked, 2)) {
				LOG("Unacked buffer overflowing. Dropping packet\n");
				return;
			}

			data_len = packetbuf_datalen() - HEADER_SIZE;
			hops = NUM_HOPS(dp);

			sp = (struct slim_packet*)queue_buffer_find(&c->dupe, p,
					slim_packet_to_packet_cmp);

			if (sp != NULL) {
				/* This is a dupe packet, we've seen this before, perhaps from
				 * another forwarder than who now sent it. */

				instructions = c->cb->dupe_recv(c, &dp->hdr.originator,
						&dp->hdr.sender, hops, dp->data, data_len);
			} else {
				instructions = c->cb->recv(c, &dp->hdr.originator, &dp->hdr.sender,
						hops, dp->data, data_len);
			}

			if (instructions & FORWARD_PACKET) {
				struct buffered_packet *forward_bp =
					packet_buffer_data_packet(&c->unacked, dp, data_len,
							MSG_PRIO_FORWARD_MSG);

				if (forward_bp == NULL) {
					/* user sent packet(s) while callbacking, which made us run
					 * out of buffer space.
					 */
					LOG("Forwarding packet failed. Buffer space dep.\n");
				} else {
					struct packet *forward_p =
						packet_buffer_get_packet(forward_bp);
					++hops;
					SET_HOPS(forward_p, hops);
					rimeaddr_copy(&forward_p->hdr.sender,
							&rimeaddr_node_addr);
				}
			} else if (instructions & DROP_PACKET) {
				/* do nthing */
				LOG("Dropping packet (user request).\n");
			}

			if (instructions & FORWARD_STORED_PACKET) {
				if (c->stored_packet.initialized) {
					struct buffered_packet *forward_sbp =
						packet_buffer_packet(&c->unacked, &c->stored_packet.p,
								&c->stored_packet.data,
								c->stored_packet.data_len,
								MSG_PRIO_FORWARD_MSG);
					if (forward_sbp == NULL) {
						LOG("Error forwarding stored packet: Send buffer full\n");
					}
				} else {
					LOG("Error forwarding stored packet: Stored packet not initialized\n");
				}
			}

			if (instructions & STORE_PACKET_FOR_FORWARDING) {
				c->stored_packet.initialized = 1;
				c->stored_packet.data_len = data_len;
				memcpy(&c->stored_packet.p, p, HEADER_SIZE + data_len);
				LOG("Storing packet for forwarding.\n");
			}

			if (instructions & STORE_PACKET_FOR_DUPE_CHECKS) {
				sp = (struct slim_packet*)queue_buffer_alloc_back(&c->dupe);
				if (sp == NULL) {
					queue_buffer_pop_front(&c->dupe, NULL);
					sp = (struct slim_packet*)queue_buffer_alloc_back(&c->dupe);
				}

				rimeaddr_copy(&sp->originator, &dp->hdr.originator);
				sp->originator_seqno = dp->hdr.originator_seqno;
				LOG("Storing packet for dupe checks.\n");
			}
		} else {
			/* Recieved our own eariler sent packet. */
			LOG("Recieved our own packet. Dropping.\n");
		}

		/* make ack packet */
		{
			struct ack_packet ap;
			struct buffered_packet *buffered_ack;

			SET_ACK(&ap);
			rimeaddr_copy(&ap.hdr.sender, &rimeaddr_node_addr);
			rimeaddr_copy(&ap.hdr.originator, &p->hdr.originator);
			ap.hdr.originator_seqno = p->hdr.originator_seqno;
			rimeaddr_copy(&ap.destination, &p->hdr.sender);

			buffered_ack = packet_buffer_ack_packet(&c->unacked, &ap,
					MSG_PRIO_ACK);
			if (buffered_ack == NULL) {
				LOG("ACK packet failed: no buffer space\n");
			} else {
				LOG("Making ACK packet.\n");
			}
		}

		set_timer_for_next_packet(c);
	}
}


/* TODO(johan): use two different callbacks? one for ack, one for data. */
static const struct abc_callbacks abc_cb = {abc_recv};

int rfnr_is_neighbor(struct rfnr_conn *c, const rimeaddr_t *naddr) {
	return neighbors_classify_neighbor(&c->ns, naddr) == IS_NEIGHBOR;
}

void rfnr_open(struct rfnr_conn *c, uint16_t data_channel, 
		uint16_t ack_channel, const struct rfnr_callbacks *cb) {
	LOG("OPENING rfnr\n");
	abc_open(&c->data_conn, data_channel, &abc_cb);
	abc_open(&c->ack_conn, ack_channel, &abc_cb);

	PACKET_BUFFER_INIT_WITH_STRUCT(c, unacked, UNACKED_PACKETS_BUF_LENGTH);
	QUEUE_BUFFER_INIT_WITH_STRUCT(c, dupe, SLIM_PACKET_SIZE,
			DUPE_CHECK_PACKETS_BUF_LENGTH);

	c->stored_packet.initialized = 0;

	neighbors_init(&c->ns);
	c->seqno = 0;
	c->cb = cb;
}

void rfnr_close(struct rfnr_conn *c) {
	LOG("CLOSING rfnr\n");
	abc_close(&c->data_conn);
	abc_close(&c->ack_conn);
}


void rfnr_async_send(struct rfnr_conn *c, const void *data, uint8_t data_len) {
	struct packet p = {{0}};

	ASSERT(data_len <= MAX_DATA_SIZE);
	
	SET_DATA(&p);
	SET_HOPS(&p, 1);
	rimeaddr_copy(&p.hdr.sender, &rimeaddr_node_addr);
	rimeaddr_copy(&p.hdr.originator, &rimeaddr_node_addr);
	p.hdr.originator_seqno = c->seqno++;

	packet_buffer_packet(&c->unacked, &p, data, data_len,
			MSG_PRIO_USER_MSG);

	LOG("Queuing user packet.\n");
	set_timer_for_next_packet(c);
}
