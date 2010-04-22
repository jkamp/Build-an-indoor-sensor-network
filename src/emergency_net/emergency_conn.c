#include "emergency_net/emergency_conn.h"

#include "lib/random.h"

#include "base/log.h"

#define MAX_TIMES_SENT 5
#define SENDING_ACK_INTERVAL_MIN 0
#define SENDING_ACK_INTERVAL_MAX 100
#define SENDING_DATA_INTERVAL_MIN 300
#define SENDING_DATA_INTERVAL_MAX 400

enum {
	MSG_PRIO_ACK = HIGHEST_PRIO, /* highest prio */
	MSG_PRIO_USER_MSG = HIGHEST_PRIO + 1
};


static void send_ack(void *ptr);
static void send_data(void *ptr);

static void set_timer_for_next_packet(struct ec *c) {
	struct buffered_packet *bp =
		packet_buffer_get_packet_for_sending(&c->sq);

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
				struct neighbors *ns = packet_buffer_unacked_neighbors(bp);
				struct neighbor_node *nn;

				for (nn = neighbors_begin(ns); nn != NULL;
								nn = neighbors_next(ns)) {
						neighbors_warn(ns, &nn->addr);
				}

				LOG("Packet has been sent too many times without ACKs, neighbor "
						"presumed dead. Dropping packet from further sending.\n");

				packet_buffer_free(&c->sq, bp);
				bp = packet_buffer_get_packet_for_sending(&c->sq);
				if (bp == NULL) {
						ctimer_stop(&c->send_timer);
						return;
				}
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
	struct ec *c = (struct ec*)ptr;
	struct unicast_packet *p = (struct unicast_packet*)
			packet_buffer_get_packet(c->next_to_send);

	packetbuf_clear();
	packetbuf_set_datalen(UNICAST_PACKET_HDR_SIZE);
	memcpy(packetbuf_dataptr(), p, UNICAST_PACKET_HDR_SIZE);

	LOG("[PCKT SEND] type:A, hops:%d, s:%d.%d, o:%d.%d, seqno:%d\n",
			p->hdr.hops, p->hdr.sender.u8[0], p->hdr.sender.u8[1],
			p->hdr.originator.u8[0], p->hdr.originator.u8[1],
			p->hdr.seqno);
	if (abc_send(&c->ack_conn) == 0) {
		LOG("ERROR: ACK packet could not be sent.\n");
		set_timer_for_next_packet(c);
		return;
	}

	packet_buffer_free(&c->sq, c->next_to_send);

	set_timer_for_next_packet(c);
}

static void send_data(void *ptr) {
	struct ec *c = (struct ec*)ptr;
	const struct packet *p = packet_buffer_get_packet(c->next_to_send);
	packetbuf_clear();

	switch(PACKET_TYPE(p)) {
		case BROADCAST: 
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
						struct neighbors *ns =
								packet_buffer_unacked_neighbors(c->next_to_send);
						uint8_t nsize = neighbors_size(ns);

						if (nsize > 1) {
								/* make multicast */

								struct multicast_packet *mp = (struct multicast_packet*)
										packetbuf_dataptr();
								rimeaddr_t *addr = (rimeaddr_t*)mp->data;
								struct neighbors *ns = packet_buffer_unacked_neighbors(c->next_to_send);
								struct neighbor_node *n;

								packetbuf_set_datalen(MULTICAST_PACKET_HDR_SIZE+
												nsize*sizeof(rimeaddr_t)+
												packet_buffer_data_len(c->next_to_send));

								init_multicast_packet(mp, DATA, p->hdr.hops,
										&p->hdr.originator, &p->hdr.sender, p->hdr.seqno,
										nsize);

								for(n = neighbors_begin(ns); n != NULL;
												n = neighbors_next(ns)) {
										rimeaddr_copy(addr++, &n->addr);
								}
								memcpy(addr, p->data, packet_buffer_data_len(c->next_to_send));
						} else {
								/* make unicast */
								struct unicast_packet *up = (struct unicast_packet*)
										packetbuf_dataptr();

								packetbuf_set_datalen(UNICAST_PACKET_HDR_SIZE+
												packet_buffer_data_len(c->next_to_send));

								init_unicast_packet(up, DATA, p->hdr.hops, &p->hdr.originator,
										&p->hdr.sender, p->hdr.seqno, &(neighbors_begin(ns)->addr));
								memcpy(up->data, p->data, packet_buffer_data_len(c->next_to_send));
						}
				} else {
						/* make broadcast */
						uint8_t len = BROADCAST_PACKET_HDR_SIZE+
								packet_buffer_data_len(c->next_to_send);
						struct broadcast_packet *bp = (struct broadcast_packet*)
								packetbuf_dataptr();
						packetbuf_set_datalen(len);
						memcpy(bp, p, len);
				}
				break;
		case MULTICAST:
				ASSERT(0);
				break;
		case UNICAST:
				ASSERT(0);
				break;
	}

	if (abc_send(&c->data_conn) == 0) {
		LOG("ERROR: DATA packet could not be sent.\n");
	} else {
			packet_buffer_increment_times_sent(c->next_to_send);
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

		LOG("Storing packet for dupe checks.\n");
	}
}

//void check_neighbors(const struct ec *c)
//{
//	struct neighbor_node *nn;
//	LOG("Neighbors: ");
//	for( nn = neighbors_begin(&c->ns); nn != NULL; nn = neighbors_next(&c->ns)) {
//		LOG("%d.%d, ", nn->addr.u8[0], nn->addr.u8[1]);
//	}
//	LOG("\n");
//}

static void packet_recv(struct abc_conn *bc) {
	const struct packet *p = (struct packet*)packetbuf_dataptr();
	const uint8_t *data = NULL;
	uint8_t data_len = 0;
	int8_t is_for_us = 0;
	int8_t is_ack = 0;
	struct ec *c;

	if (IS_ACK(p)) {
		c = (struct ec*)(bc-1);
		is_ack = 1;
	} else {
		c = (struct ec*)bc;
	}
	
	
	switch(PACKET_TYPE(p)) {
		case BROADCAST:
			{
				is_for_us = 1;
				data = p->data;
				data_len = packetbuf_datalen() - BROADCAST_PACKET_HDR_SIZE;
			}
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

	if (is_for_us && neighbors_is_neighbor(&c->ns, &p->hdr.sender)) {
		//rtimer_clock_t timediff = neighbors_update_time_diff(&c->ns, &p->hdr.sender);
		if (is_ack) {
			/* ec.ack_conn is one down, correct it. */
			struct buffered_packet *bp;

			LOG("[PCKT RECV ACK] ");
			DEBUG_PACKET(p);

			bp = packet_buffer_find_packet(&c->sq, p, originator_seqno_cmp);

			if (bp != NULL) {
				neighbors_remove(&bp->unacked_ns, &p->hdr.sender);
				if (neighbors_size(&bp->unacked_ns) == 0) {
					/* every neighbor acked our packet. */
					LOG("Every neighbor acked packet.\n");
					packet_buffer_free(&c->sq, bp);
					ASSERT(packet_buffer_find_packet(&c->sq, p, originator_seqno_cmp) == NULL);
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
					LOG("[PCKT RECV DUPE] ");
					DEBUG_PACKET(p);
					is_dupe = 1;

				} else {
					LOG("[PCKT RECV] ");
					DEBUG_PACKET(p);
				}

				/* check if we have atleast room for two packets (one ACK, one
				 * forward request from user) */
				if (packet_buffer_has_room_for_packets(&c->sq, 2)) {
					if(is_dupe) {
						c->cb->recv_dupe(c, &p->hdr.originator, &p->hdr.sender, p->hdr.hops,
								p->hdr.seqno, data, data_len);
					} else {
						c->cb->recv(c, &p->hdr.originator, &p->hdr.sender, p->hdr.hops,
								p->hdr.seqno, data, data_len);
						store_packet_for_dupe_checks(c, p);
					}
				} else {
					LOG("Unacked buffer overflowing. Dropping packet\n");
					make_ack = 0;
				}

				if (make_ack) {
					/* Make ACK packet. */
					struct unicast_packet ap;
					init_unicast_packet(&ap, ACK, 0, &p->hdr.originator,
							&rimeaddr_node_addr, p->hdr.seqno, &p->hdr.sender);
					/* Check so that we dont get duplicates of the same ack
					 * in the sending queue. No need for that. */
					if (packet_buffer_find_packet(&c->sq, (struct packet*)&ap, unicast_packet_cmp) == NULL) {
						packet_buffer_unicast_packet(&c->sq, &ap, NULL, 0, &c->ns, MSG_PRIO_ACK);
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


void ec_async_broadcast(struct ec *c, const rimeaddr_t *originator, 
		const rimeaddr_t *sender, uint8_t hops, uint8_t seqno, const void *data,
		uint8_t data_len) {

	struct broadcast_packet bp;

	init_broadcast_packet(&bp, DATA, hops, originator, sender, seqno);
	LOG("Broadcast packet: ");
	DEBUG_PACKET((&bp));

	packet_buffer_broadcast_packet(&c->sq, &bp, data, data_len, &c->ns, MSG_PRIO_USER_MSG);

	store_packet_for_dupe_checks(c, (struct packet*)&bp);

	set_timer_for_next_packet(c);
}

static const struct abc_callbacks abc_cb = {packet_recv};

void ec_open(struct ec *c, uint16_t data_channel, 
		const struct ec_callbacks *cb) {

	abc_open(&c->data_conn, data_channel, &abc_cb);
	abc_open(&c->ack_conn, data_channel+1, &abc_cb);

	PACKET_BUFFER_INIT_WITH_STRUCT(c, sq, SENDING_QUEUE_LENGTH);
	QUEUE_BUFFER_INIT_WITH_STRUCT(c, dq, SLIM_PACKET_SIZE, DUPE_QUEUE_LENGTH);

	neighbors_init(&c->ns);
	c->cb = cb;
}

void ec_close(struct ec *c) {
	LOG("CLOSING rfnr\n");
	abc_close(&c->data_conn);
	abc_close(&c->ack_conn);
}

void ec_update_neighbors(struct ec *c, const struct neighbors *ns) {
	neighbors_copy(&c->ns, ns);
}
