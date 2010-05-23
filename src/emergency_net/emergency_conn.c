#include "emergency_net/emergency_conn.h"

#include "lib/random.h"

#include "net/rime/timesynch.h"

#include "emergency_net/packet.h"
#include "emergency_net/timesynch_gluer.h"

#include "base/log.h"

#define MAX_TIMES_SENT 5
#define SENDING_ACK_INTERVAL_MIN 0
#define SENDING_ACK_INTERVAL_MAX 100
#define SENDING_DATA_INTERVAL_MIN 100
#define SENDING_DATA_INTERVAL_MAX 200

#define TIMESYNCH_LEADER_UPDATE (30*CLOCK_SECOND)
#define TIMESYNCH_LEADER_TIMEOUT (60*CLOCK_SECOND)

enum {
	MSG_PRIO_ACK = HIGHEST_PRIO,
	MSG_PRIO_USER_MSG = HIGHEST_PRIO + 1,
	MSG_PRIO_TIMESYNCH_MSG = HIGHEST_PRIO + 2
};

static void async_send_broadcast_data(void *ptr);
static void async_send_neighbor_ack(void *ptr);
static void async_send_neighbor_data(void *ptr);
static void async_send_timesynch(void *ptr);

static void send_broadcast_data(void *ptr);
static void send_neighbor_ack(void *ptr);
static void send_neighbor_data(void *ptr);
static void send_timesynch(void *ptr);

static void set_timer_for_next_packet(struct ec *c) {
	struct buffered_packet *bp =
		packet_buffer_get_packet_for_sending(&c->sq);

	if (bp != NULL) {
		const struct packet* p = packet_buffer_get_packet(bp);
		c->next_to_send = bp;
		LOG("[NEXT TO SEND]: ");
		DEBUG_PACKET(p);
	} else {
			ctimer_stop(&c->send_timer);
	}
}

static void async_send_broadcast_data(void *ptr) {
	struct ec *c = (struct ec*)ptr;
	ctimer_set(&c->send_timer, SENDING_DATA_INTERVAL_MIN + 
			random_rand()%
			(SENDING_DATA_INTERVAL_MAX-SENDING_DATA_INTERVAL_MIN),
			&send_broadcast_data, ptr);
}
static void async_send_neighbor_ack(void *ptr) {
	struct ec *c = (struct ec*)ptr;
	ctimer_set(&c->send_timer, SENDING_ACK_INTERVAL_MIN + 
			random_rand()%
			(SENDING_ACK_INTERVAL_MAX-SENDING_ACK_INTERVAL_MIN),
			&send_neighbor_ack, ptr);
}
static void async_send_neighbor_data(void *ptr) {
	struct ec *c = (struct ec*)ptr;
	while (packet_buffer_times_sent(c->next_to_send) >= MAX_TIMES_SENT) {
		LOG("Packet has been sent too many times without ACKs, neighbor "
				"presumed dead. Dropping packet from further sending.\n");
		/* TODO: implement warn. */

		packet_buffer_free(&c->sq, c->next_to_send);
		c->next_to_send = packet_buffer_get_packet_for_sending(&c->sq);
		if (c->next_to_send == NULL) {
			ctimer_stop(&c->send_timer);
			return;
		}
	}

	ctimer_set(&c->send_timer, SENDING_DATA_INTERVAL_MIN + 
			random_rand()%
			(SENDING_DATA_INTERVAL_MAX-SENDING_DATA_INTERVAL_MIN),
			&send_neighbor_data, ptr);
}
static void async_send_timesynch(void *ptr) {
	struct ec *c = (struct ec*)ptr;
	ctimer_set(&c->send_timer, SENDING_DATA_INTERVAL_MIN + 
			random_rand()%
			(SENDING_DATA_INTERVAL_MAX-SENDING_DATA_INTERVAL_MIN),
			send_timesynch, c);
}

static void send_broadcast_data(void *ptr) {
	struct ec *c = (struct ec*)ptr;
	const struct packet *p = (struct packet*)
		packet_buffer_get_packet(c->next_to_send);
	struct broadcast_packet_ns *bp = (struct broadcast_packet_ns*)
		packetbuf_dataptr();
	uint8_t len = BROADCAST_PACKET_HDR_SIZE+
		packet_buffer_data_len(c->next_to_send);

	LOG("[BROADCAST DATA SEND]: ");
	DEBUG_PACKET(p);

	packetbuf_clear();
	packetbuf_set_datalen(len);
	memcpy(bp, p, len);

	if (abc_send(&c->broadcast_conn) == 0) {
		LOG("ERROR: ACK packet could not be sent.\n");
	} else {
		packet_buffer_free(&c->sq, c->next_to_send);
	}

	set_timer_for_next_packet(c);
}

static void send_neighbor_ack(void *ptr) {
	struct ec *c = (struct ec*)ptr;
	struct unicast_packet *p = (struct unicast_packet*)
			packet_buffer_get_packet(c->next_to_send);

	LOG("[ACK SEND]: ");
	DEBUG_PACKET(p);

	packetbuf_clear();
	packetbuf_set_datalen(UNICAST_PACKET_HDR_SIZE);
	memcpy(packetbuf_dataptr(), p, UNICAST_PACKET_HDR_SIZE);


	if (abc_send(&c->neighbor_conn) == 0) {
		LOG("ERROR: ACK packet could not be sent.\n");
	} else {
		packet_buffer_free(&c->sq, c->next_to_send);
	}

	set_timer_for_next_packet(c);
}



static void send_neighbor_data(void *ptr) {
	struct ec *c = (struct ec*)ptr;
	const struct packet *p = (struct packet*)
		packet_buffer_get_packet(c->next_to_send);

	LOG("[DATA SEND]: ");
	DEBUG_PACKET(p);

	packetbuf_clear();

	if(IS_PACKET_FLAG_SET(p, BROADCAST)){
		if (packet_buffer_times_sent(c->next_to_send) > 0) {
			/* If the packet has been sent more than once, transform it into a
			 * either a multicast or unicast packet. This to enable a more
			 * efficient broadcasting to target the neighbors which have not
			 * acked the packet. This protects against if a neighbor's ACK has
			 * been lost. By specifying this by target multicasting we can let
			 * the neighbors know that they should resend the ACKs. If we just
			 * broadcasted we would not show whose ACK went missing. And
			 * sending multiple unicasts to the ones whose ACKs are missing is
			 * less efficient. */
			uint8_t nsize = packet_buffer_num_unacked_neighbors(c->next_to_send);
			ASSERT(nsize != 0);

			if (nsize > 1) {
				/* make multicast */

				struct multicast_packet *mp = (struct multicast_packet*)
					packetbuf_dataptr();
				rimeaddr_t *addr = (rimeaddr_t*)mp->data;
				const rimeaddr_t *i = packet_buffer_unacked_neighbors_begin(c->next_to_send);

				packetbuf_set_datalen(MULTICAST_PACKET_HDR_SIZE+
						nsize*sizeof(rimeaddr_t)+
						packet_buffer_data_len(c->next_to_send));

				init_multicast_packet(mp, 0, p->hdr.hops,
						&p->hdr.originator, &p->hdr.sender, p->hdr.seqno,
						nsize);

				for(; i != NULL; i = packet_buffer_unacked_neighbors_next(c->next_to_send)) {
					rimeaddr_copy(addr++, i);
				}
				memcpy(addr, p->data, packet_buffer_data_len(c->next_to_send));
			} else {
				/* make unicast */
				struct unicast_packet *up = (struct unicast_packet*)
					packetbuf_dataptr();
				const rimeaddr_t *addr = packet_buffer_unacked_neighbors_begin(c->next_to_send);
				ASSERT(addr != NULL)

				packetbuf_set_datalen(UNICAST_PACKET_HDR_SIZE+
						packet_buffer_data_len(c->next_to_send));

				init_unicast_packet(up, 0, p->hdr.hops, &p->hdr.originator,
						&p->hdr.sender, p->hdr.seqno, addr);
				memcpy(up->data, p->data, packet_buffer_data_len(c->next_to_send));
			}
		} else {
			/* make broadcast */
			uint8_t len = BROADCAST_PACKET_HDR_SIZE+
				packet_buffer_data_len(c->next_to_send);
			struct broadcast_packet_ns *bp = (struct broadcast_packet_ns*)
				packetbuf_dataptr();
			packetbuf_set_datalen(len);
			memcpy(bp, p, len);
		}
	} else {
		ASSERT(0);
	}


	if (abc_send(&c->neighbor_conn) == 0) {
		LOG("ERROR: DATA packet could not be sent.\n");
	} else {
		packet_buffer_increment_times_sent(c->next_to_send);
	}

	set_timer_for_next_packet(c);
}

static void send_timesynch(void *ptr) {
	struct ec *c = (struct ec*)ptr;
	struct broadcast_packet *bp = (struct broadcast_packet*)
		packet_buffer_get_packet(c->next_to_send);

	LOG("[TIMESYNCH SEND]: ");
	DEBUG_PACKET(bp);

	packetbuf_clear();
	packetbuf_set_datalen(BROADCAST_PACKET_HDR_SIZE);
	memcpy(packetbuf_dataptr(), bp, BROADCAST_PACKET_HDR_SIZE);

	if (abc_send(&c->timesynch_conn) == 0) {
		LOG("ERROR: Timesynch packet could not be sent.\n");
	} else {
		packet_buffer_free(&c->sq, c->next_to_send);
	}

	set_timer_for_next_packet(c);
}

static void store_packet_for_dupe_checks(struct ec *c, const struct packet *p) {

	struct slim_packet *sp = (struct slim_packet*)queue_buffer_find(&c->dq,
			p, slim_packet_to_packet_cmp);

	if (sp == NULL) {
		 sp = (struct slim_packet*) queue_buffer_alloc_front(&c->dq);

		if (sp == NULL) {
			queue_buffer_pop_back(&c->dq, NULL);
			sp = (struct slim_packet*)queue_buffer_alloc_front(&c->dq);
			ASSERT(sp != NULL);
		}

		rimeaddr_copy(&sp->originator, &p->hdr.originator);
		sp->seqno = p->hdr.seqno;
	}
}

static void neighbor_recv(struct abc_conn *bc) {
	const struct packet *p = (struct packet*)packetbuf_dataptr();
	struct ec *c = (struct ec*)bc;
	const uint8_t *data = NULL;
	uint8_t data_len = 0;
	int8_t is_for_us = 0;
	
	switch(PACKET_TYPE(p)) {
		case BROADCAST:
			is_for_us = 1;
			data = p->data;
			data_len = packetbuf_datalen() - BROADCAST_PACKET_HDR_SIZE;
			break;

		case MULTICAST:
			{
				const struct multicast_packet *mp = (struct multicast_packet*)p;
				rimeaddr_t *to = (rimeaddr_t*)mp->data;
				int i;
				for (i = 0; i < mp->num_ids; ++i) {
					if (rimeaddr_cmp(to++, &rimeaddr_node_addr)) {
						uint8_t ids_size = sizeof(rimeaddr_t)*mp->num_ids;
						data = mp->data + ids_size;
						data_len = packetbuf_datalen() -
							MULTICAST_PACKET_HDR_SIZE - ids_size;
						is_for_us = 1;
						break;
					}
				}
			}
			break;
		case UNICAST:
			{
				const struct unicast_packet *up = (struct unicast_packet*)p;
				if (rimeaddr_cmp(&up->destination, &rimeaddr_node_addr)) {
					is_for_us = 1;
					data = up->data;
					data_len = packetbuf_datalen() - UNICAST_PACKET_HDR_SIZE;
				}
			}
			break;
		default:
			ASSERT(0);
	}

	if (is_for_us && neighbors_is_neighbor(c->ns, &p->hdr.sender)) {
		if (IS_PACKET_FLAG_SET(p, ACK)) {
			struct buffered_packet *bp;

			LOG("[NEIGHBOR ACK RECV] ");
			DEBUG_PACKET(p);

			bp = packet_buffer_find_buffered_packet(&c->sq, p, originator_seqno_cmp);

			if (bp != NULL) {
				packet_buffer_neighbor_acked(bp, &p->hdr.sender);
				if (packet_buffer_all_neighbors_acked(bp)) {
					LOG("Every neighbor acked packet.\n");
					packet_buffer_free(&c->sq, bp);
				}
			} else {
				LOG("Recived ack for non-sent packet\n");
			}
		} else {
			/* Data packet */
			int8_t make_ack = 1;
			int8_t is_dupe = 0;

			struct slim_packet *sp = (struct slim_packet*)
				queue_buffer_find(&c->dq, p, slim_packet_to_packet_cmp);
			if (sp != NULL) {
				/* dupe packet */
				LOG("[NEIGHBOR DATA RECV DUPE] ");
				DEBUG_PACKET(p);
				is_dupe = 1;

			} else {
				LOG("[NEIGHBOR DATA RECV] ");
				DEBUG_PACKET(p);
			}

			if (!is_dupe) {
				/* check if we have atleast room for three packets (one ACK, one
				 * forward request from user, one data packet from user) */
				if(packet_buffer_has_room_for_packets(&c->sq, 3)) {
					c->cb->neighbor_recv(c, &p->hdr.originator, &p->hdr.sender,
							p->hdr.hops, p->hdr.seqno, data, data_len);
					store_packet_for_dupe_checks(c, p);
				} else {
					LOG("Packet buffer overflowing. Dropping packet\n");
					make_ack = 0;
				}
			}

			if (make_ack) {
				/* Make ACK packet. */
				struct unicast_packet ap;
				init_unicast_packet(&ap, ACK, 0, &p->hdr.originator,
						&rimeaddr_node_addr, p->hdr.seqno, &p->hdr.sender);
				/* Check so that we dont get duplicates of the same ack
				 * in the sending queue. No need for that. */
				if (packet_buffer_find_buffered_packet(&c->sq, 
							(struct packet*)&ap, unicast_packet_cmp) == NULL) {
					packet_buffer_unicast_packet(&c->sq, &ap, NULL, 0, NULL,
							&async_send_neighbor_ack, MSG_PRIO_ACK);
				} else {
					LOG("ACK ALREADY IN SENDING QUEUE\n");
				}
			}
		}
	} else {
		LOG("NOT FOR US OR NOT A NEIGHBOR\n");
	}

	set_timer_for_next_packet(c);
}

static void broadcast_recv(struct abc_conn *bc) {
	const struct packet *p = (struct packet*)packetbuf_dataptr();
	struct ec *c = (struct ec*)(bc-2);
	uint8_t data_len = packetbuf_datalen() - BROADCAST_PACKET_HDR_SIZE;

	struct slim_packet *sp = (struct slim_packet*)
		queue_buffer_find(&c->dq, p, slim_packet_to_packet_cmp);
	if (sp != NULL) {
		/* dupe packet */
		LOG("[BROADCAST RECV DUPE] ");
		DEBUG_PACKET(p);

	} else {
		LOG("[BROADCAST DATA RECV] ");
		DEBUG_PACKET(p);
		c->cb->broadcast_recv(c, &p->hdr.originator, &p->hdr.sender,
				p->hdr.hops, p->hdr.seqno, p->data, data_len);
		store_packet_for_dupe_checks(c, p);
	}
}

void ec_async_broadcast_ns(struct ec *c, const rimeaddr_t *originator, 
		const rimeaddr_t *sender, uint8_t hops, uint8_t seqno, const void *data,
		uint8_t data_len) {

	struct broadcast_packet bp;

	init_broadcast_packet(&bp, 0, hops, originator, sender, seqno);

	packet_buffer_broadcast_packet(&c->sq, &bp, data, data_len, c->ns,
			&async_send_neighbor_data, MSG_PRIO_USER_MSG);

	store_packet_for_dupe_checks(c, (struct packet*)&bp);

	set_timer_for_next_packet(c);
}

void ec_async_broadcast(struct ec *c, const rimeaddr_t *originator, 
		const rimeaddr_t *sender, uint8_t hops, uint8_t seqno, const void *data,
		uint8_t data_len) {

	struct broadcast_packet bp;

	init_broadcast_packet(&bp, 0, hops, originator, sender, seqno);

	packet_buffer_broadcast_packet(&c->sq, &bp, data, data_len, NULL,
			&async_send_broadcast_data, MSG_PRIO_USER_MSG);

	store_packet_for_dupe_checks(c, (struct packet*)&bp);

	set_timer_for_next_packet(c);
}

static void timesynch(void *ptr) {
	struct ec *c = (struct ec*)ptr;
	struct broadcast_packet bp;
	set_authority_level(timesynch_rimeaddr_to_authority(&rimeaddr_node_addr));
	set_authority_seqno(c->ts.seqno);

	init_broadcast_packet(&bp, TIMESYNCH, 0, &rimeaddr_node_addr,
			&rimeaddr_node_addr, c->ts.seqno++);

	packet_buffer_clear_priority(&c->sq, MSG_PRIO_TIMESYNCH_MSG);

	packet_buffer_broadcast_packet(&c->sq, &bp, NULL, 0, NULL,
			&async_send_timesynch, MSG_PRIO_TIMESYNCH_MSG);
	LOG("[TIMESYNCH TIMEOUT]: ");
	DEBUG_PACKET(&bp);

	set_timer_for_next_packet(c);
	ctimer_set(&c->ts.timer, TIMESYNCH_LEADER_UPDATE, timesynch, c);
	
	c->cb->timesynch(c);
}

static void timesynch_recv(struct abc_conn *bc) {
	struct ec *c = (struct ec*)(bc-1);
	if (c->ts.is_on) {
		const struct packet *p = (struct packet*)packetbuf_dataptr();
		authority_t authlevel = timesynch_rimeaddr_to_authority(&p->hdr.originator);

		ASSERT(IS_PACKET_FLAG_SET(p, TIMESYNCH));
		LOG("[TIMESYNCH RECV]: ");
		DEBUG_PACKET(p);

		if (authlevel < authority_level) {
			struct broadcast_packet bp;
			authority_t my_authlevel = 
				timesynch_rimeaddr_to_authority(&rimeaddr_node_addr);

			packet_buffer_clear_priority(&c->sq, MSG_PRIO_TIMESYNCH_MSG);

			if (my_authlevel < authlevel) {
				timesynch(c);
			} else {
				set_authority_level(authlevel);
				set_authority_seqno(p->hdr.seqno);

				init_broadcast_packet(&bp, TIMESYNCH, p->hdr.hops+1, 
						&p->hdr.originator, &rimeaddr_node_addr, p->hdr.seqno);

				LOG("New leader. Forwarding timesynch packet: ");
				DEBUG_PACKET((&bp));

				packet_buffer_broadcast_packet(&c->sq, &bp, NULL, 0, NULL,
						&async_send_timesynch, MSG_PRIO_TIMESYNCH_MSG);

				set_timer_for_next_packet(c);
				ctimer_set(&c->ts.timer, TIMESYNCH_LEADER_TIMEOUT, timesynch, c);
			}

			c->cb->timesynch(c);
		} else if (authlevel == authority_level) {
			if (p->hdr.seqno > authority_seqno || 
					p->hdr.seqno < authority_seqno/8 /*overflow*/ ) {
				/* new timesynch recieved */
				struct broadcast_packet bp;
				set_authority_seqno(p->hdr.seqno);

				init_broadcast_packet(&bp, TIMESYNCH, p->hdr.hops+1, 
						&p->hdr.originator, &rimeaddr_node_addr, p->hdr.seqno);

				LOG("Forwarding timesynch packet: ");
				DEBUG_PACKET((&bp));

				packet_buffer_clear_priority(&c->sq, MSG_PRIO_TIMESYNCH_MSG);

				packet_buffer_broadcast_packet(&c->sq, &bp, NULL, 0, NULL,
						&async_send_timesynch, MSG_PRIO_TIMESYNCH_MSG);

				set_timer_for_next_packet(c);

				ctimer_set(&c->ts.timer, TIMESYNCH_LEADER_TIMEOUT, timesynch, c);

				c->cb->timesynch(c);
			} 
		}
	}
}

static const struct abc_callbacks neighbor_cb = {neighbor_recv};
static const struct abc_callbacks timesynch_cb = {timesynch_recv};
static const struct abc_callbacks broadcast_cb = {broadcast_recv};

void ec_open(struct ec *c, uint16_t data_channel, 
		const struct ec_callbacks *cb) {

	timesynch_init();

	abc_open(&c->neighbor_conn, data_channel, &neighbor_cb);
	abc_open(&c->timesynch_conn, data_channel+1, &timesynch_cb);
	abc_open(&c->broadcast_conn, data_channel+2, &broadcast_cb);

	PACKET_BUFFER_INIT_WITH_STRUCT(c, sq, SENDING_QUEUE_LENGTH);
	QUEUE_BUFFER_INIT_WITH_STRUCT(c, dq, SLIM_PACKET_SIZE, DUPE_QUEUE_LENGTH);

	c->ts.is_on = 0;

	c->cb = cb;
}

void ec_close(struct ec *c) {
	LOG("CLOSING rfnr\n");
	abc_close(&c->neighbor_conn);
	abc_close(&c->timesynch_conn);
	abc_close(&c->broadcast_conn);
}

void ec_set_neighbors(struct ec *c, const struct neighbors *ns) {
	c->ns = ns;
}


void ec_timesynch_network(struct ec *c) {
	ASSERT(c->ts.is_on == 1);
	if(ctimer_expired(&c->ts.timer)) {
		timesynch(c);
	}
}

void ec_timesynch_on(struct ec *c) {
	c->ts.is_on = 1;
	c->ts.seqno = 0;
	set_authority_level(AUTHORITY_LEVEL_MAX);
	set_authority_seqno(0);
}

void ec_timesynch_off(struct ec *c) {
	ctimer_stop(&c->ts.timer);
	c->ts.is_on = 0;
}
