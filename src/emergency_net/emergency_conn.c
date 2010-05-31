#include "emergency_net/emergency_conn.h"

#include "lib/random.h"

#include "net/rime/timesynch.h"

#include "emergency_net/packet.h"
#include "emergency_net/timesynch_gluer.h"

#include "base/log.h"

#include <stddef.h> /* For offsetof */

#define MAX_TIMES_SENT 5

#define FAST_TRANSMIT (CLOCK_SECOND+random_rand()%(2*CLOCK_SECOND))
#define FAST_TRANSMIT_ACK (random_rand()%CLOCK_SECOND)

#define RETRANSMIT_NEIGHBOR_DATA (6*CLOCK_SECOND)

#define TIMESYNCH_LEADER_UPDATE (30*CLOCK_SECOND)
#define TIMESYNCH_LEADER_TIMEOUT (60*CLOCK_SECOND)

enum {
	MSG_TYPE_NEIGHBOR_ACK = PACKET_BUFFER_TYPE_ZERO,
	MSG_TYPE_NEIGHBOR_DATA = PACKET_BUFFER_TYPE_ZERO+1,
	MSG_TYPE_BROADCAST_DATA = PACKET_BUFFER_TYPE_ZERO+2,
	MSG_TYPE_TIMESYNCH_DATA = PACKET_BUFFER_TYPE_ZERO+3,
	MSG_TYPE_MESH_DATA = PACKET_BUFFER_TYPE_ZERO+4
};

static void
print_packet_data(const uint8_t *hdr, int len)
{
  int i;

  for(i = 0; i < len; ++i) {
    LOG(" (0x%0x), ", hdr[i]);
  }

  LOG("\n");
}

static void send_neighbor_data(void *cptr) {
	struct ec *c = (struct ec*)cptr;
	struct buffered_packet *bp =
		packet_buffer_get_first_packet_from_type(&c->sq,
				MSG_TYPE_NEIGHBOR_DATA);
	if (bp != NULL) {
		const struct packet *p = (struct packet*)
			packet_buffer_get_packet(bp);

		while (packet_buffer_times_sent(bp) >= MAX_TIMES_SENT) {
			rimeaddr_t *neighbor = packet_buffer_unacked_neighbors_begin(bp);
			LOG("Packet has been sent too many times without ACKs, neighbor "
					"presumed dead. Dropping packet from further sending.\n");
			/* TODO: implement warn. */
			LOG("Unanswered neighbors: ");
			for(;neighbor != NULL; neighbor = packet_buffer_unacked_neighbors_next(bp)) {
				LOG("%d.%d, ", neighbor->u8[0], neighbor->u8[1]);
			}
			LOG("\n");

			packet_buffer_free(&c->sq, bp);
			bp = packet_buffer_get_first_packet_from_type(&c->sq,
				MSG_TYPE_NEIGHBOR_DATA);
			if (bp == NULL) {
				return;
			}
		}

		LOG("[NEIGHBOR DATA SEND]: ");
		DEBUG_PACKET(p);
		LOG("Packet data: ");
		print_packet_data(p->data, packet_buffer_data_len(bp));

		packetbuf_clear();

		if(IS_PACKET_FLAG_SET(p, BROADCAST)){
			if (packet_buffer_times_sent(bp) > 0) {
				/* If the packet has been sent more than once, transform it into a
				 * either a multicast or unicast packet. This to enable a more
				 * efficient broadcasting to target the neighbors which have not
				 * acked the packet. This protects against if a neighbor's ACK has
				 * been lost. By specifying this by target multicasting we can let
				 * the neighbors know that they should resend the ACKs. If we just
				 * broadcasted we would not show whose ACK went missing. And
				 * sending multiple unicasts to the ones whose ACKs are missing is
				 * less efficient. */
				uint8_t nsize = packet_buffer_num_unacked_neighbors(bp);
				ASSERT(nsize != 0);

				if (nsize > 1) {
					/* make multicast */
					struct multicast_packet *mp = (struct multicast_packet*)
						packetbuf_dataptr();
					rimeaddr_t *addr = (rimeaddr_t*)mp->data;
					const rimeaddr_t *i = packet_buffer_unacked_neighbors_begin(bp);

					packetbuf_set_datalen(MULTICAST_PACKET_HDR_SIZE+
							nsize*sizeof(rimeaddr_t)+
							packet_buffer_data_len(bp));

					init_multicast_packet(mp, 0, p->hdr.hops,
							&p->hdr.originator, &p->hdr.sender, p->hdr.seqno,
							nsize);

					for(; i != NULL; i = packet_buffer_unacked_neighbors_next(bp)) {
						rimeaddr_copy(addr++, i);
					}
					memcpy(addr, p->data, packet_buffer_data_len(bp));
				} else {
					/* make unicast */
					struct unicast_packet *up = (struct unicast_packet*)
						packetbuf_dataptr();
					const rimeaddr_t *addr = packet_buffer_unacked_neighbors_begin(bp);
					ASSERT(addr != NULL);

					packetbuf_set_datalen(UNICAST_PACKET_HDR_SIZE+
							packet_buffer_data_len(bp));

					init_unicast_packet(up, 0, p->hdr.hops, &p->hdr.originator,
							&p->hdr.sender, p->hdr.seqno, addr);
					memcpy(up->data, p->data, packet_buffer_data_len(bp));
				}
			} else {
				/* make broadcast */
				uint8_t len = BROADCAST_PACKET_HDR_SIZE+
					packet_buffer_data_len(bp);
				struct broadcast_packet *broadpacket = (struct broadcast_packet*)
					packetbuf_dataptr();
				packetbuf_set_datalen(len);
				memcpy(broadpacket, p, len);
			}
		} else {
			ASSERT(0);
		}

		if (abc_send(&c->neighbor_conn) == 0) {
			LOG("ERROR: DATA packet collision.\n");
			/* fast retransmit */
			ctimer_set(&c->neighbor_data_timer,
					FAST_TRANSMIT, send_neighbor_data, c);
		} else {
			packet_buffer_increment_times_sent(bp);
			ctimer_set(&c->neighbor_data_timer,
					RETRANSMIT_NEIGHBOR_DATA, send_neighbor_data, c);
		}

	}
}

static void send_neighbor_ack(void* cptr) {
	struct ec *c = (struct ec*)cptr;
	struct buffered_packet *bp =
		packet_buffer_get_first_packet_from_type(&c->sq,
				MSG_TYPE_NEIGHBOR_ACK);
	if (bp != NULL) {
		const struct unicast_packet *p = (struct unicast_packet*)
			packet_buffer_get_packet(bp);
		LOG("[NEIGHBOR ACK SEND]: ");
		DEBUG_UNICAST_PACKET(p);
		packetbuf_clear();
		packetbuf_set_datalen(UNICAST_PACKET_HDR_SIZE);
		memcpy(packetbuf_dataptr(), p, UNICAST_PACKET_HDR_SIZE);

		if (abc_send(&c->neighbor_conn) == 0) {
			LOG("ERROR: ACK packet collision.\n");
			ctimer_set(&c->neighbor_ack_timer,
					FAST_TRANSMIT_ACK,
					send_neighbor_ack, c);
		} else {
			packet_buffer_free(&c->sq, bp);
			bp = packet_buffer_get_first_packet_from_type(&c->sq,
					MSG_TYPE_NEIGHBOR_ACK);
			if (bp != NULL) {
				ctimer_set(&c->neighbor_ack_timer,
						FAST_TRANSMIT_ACK, send_neighbor_ack, c);
			}
		}
	}
}

static void send_broadcast_data(void* cptr) {
	struct ec *c = (struct ec*)cptr;
	struct buffered_packet *bp =
		packet_buffer_get_first_packet_from_type(&c->sq,
				MSG_TYPE_BROADCAST_DATA);
	if (bp != NULL) {
		const struct broadcast_packet *p = (struct broadcast_packet*)
			packet_buffer_get_packet(bp);
		struct broadcast_packet *pbuf;
		packetbuf_clear();
		pbuf = (struct broadcast_packet*) packetbuf_dataptr();

		LOG("[BROADCAST DATA SEND]: ");
		DEBUG_PACKET(p);

		packetbuf_set_datalen(BROADCAST_PACKET_HDR_SIZE+packet_buffer_data_len(bp));
		memcpy(pbuf, p, BROADCAST_PACKET_HDR_SIZE);
		memcpy(pbuf->data, p->data, packet_buffer_data_len(bp));

		if (abc_send(&c->broadcast_conn) == 0) {
			LOG("ERROR: BROADCAST DATA packet collision.\n");
			ctimer_set(&c->broadcast_data_timer,
					FAST_TRANSMIT,
					send_broadcast_data, c);
		} else {
			packet_buffer_free(&c->sq, bp);
			bp = packet_buffer_get_first_packet_from_type(&c->sq,
					MSG_TYPE_BROADCAST_DATA);
			if (bp != NULL) {
				ctimer_set(&c->broadcast_data_timer,
						FAST_TRANSMIT, send_broadcast_data, c);
			}
		}
	}
}

static void send_timesynch_data(void* cptr) {
	struct ec *c = (struct ec*)cptr;
	struct buffered_packet *bp =
		packet_buffer_get_first_packet_from_type(&c->sq,
				MSG_TYPE_TIMESYNCH_DATA);

	if (bp != NULL) {
		const struct broadcast_packet *p = (struct broadcast_packet*)
			packet_buffer_get_packet(bp);

		packetbuf_clear();

		LOG("[TIMESYNCH SEND]: ");
		DEBUG_PACKET(p);

		packetbuf_set_datalen(BROADCAST_PACKET_HDR_SIZE);
		memcpy(packetbuf_dataptr(), p, BROADCAST_PACKET_HDR_SIZE);

		if (abc_send(&c->timesynch_conn) == 0) {
			LOG("ERROR: TIMESYNCH DATA packet collision.\n");
			ctimer_set(&c->timesynch_data_timer,
					FAST_TRANSMIT,
					send_timesynch_data, c);
		} else {
			packet_buffer_free(&c->sq, bp);
			bp = packet_buffer_get_first_packet_from_type(&c->sq,
					MSG_TYPE_TIMESYNCH_DATA);
			if (bp != NULL) {
				ctimer_set(&c->timesynch_data_timer,
						FAST_TRANSMIT, send_timesynch_data, c);
			}
		}
	}
}

static void send_mesh_data(void* cptr) {
	struct ec *c = (struct ec*)cptr;
	struct buffered_packet *bp =
		packet_buffer_get_first_packet_from_type(&c->sq,
				MSG_TYPE_MESH_DATA);

	if (bp != NULL) {
		while (packet_buffer_times_sent(bp) >= MAX_TIMES_SENT) {
			LOG("Mesh packet has been sent too many times\n");
			packet_buffer_free(&c->sq, bp);
			bp = packet_buffer_get_first_packet_from_type(&c->sq,
				MSG_TYPE_MESH_DATA);
			if (bp == NULL) {
				return;
			}
		}

		packetbuf_clear();
		{
			const struct unicast_packet *p = (struct
					unicast_packet*)packet_buffer_get_packet(bp);
			struct mesh_packet *pbuf = (struct mesh_packet*)
				packetbuf_dataptr();

			LOG("[MESH SEND]: ");
			LOG("Packet data: ");
			print_packet_data(p->data, packet_buffer_data_len(bp));

			packetbuf_set_datalen(MESH_PACKET_HDR_SIZE+packet_buffer_data_len(bp));

			pbuf->seqno = p->hdr.seqno;
			memcpy(pbuf->data, p->data, packet_buffer_data_len(bp));

			packet_buffer_increment_times_sent(bp);
			if(!mesh_send(&c->meshdata_conn, &p->destination)) {
				LOG("Could not send via mesh\n");
			}
		}
	}
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
	struct ec *c = (struct ec*)((char*)bc-offsetof(struct ec, neighbor_conn));
	const uint8_t *data = NULL;
	uint8_t data_len = 0;
	int8_t is_for_us = 0;
	TRACE("[NEIGHBOR RECV] ");
	DEBUG_PACKET(p);

	
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
					LOG("Multicast to: %d.%d\n", to->u8[0],
							to->u8[1]);
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
				LOG("Unicast packet: ");
				DEBUG_UNICAST_PACKET(up);
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

	LOG("data: ");
	print_packet_data(data, data_len);

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
					/* send next neighbor packet */
					ctimer_set(&c->neighbor_data_timer,
							FAST_TRANSMIT, send_neighbor_data, c);
				}
			} else {
				LOG("Recived ack for non-sent packet\n");
			}
		} else {
			/* Data packet */
			int8_t send_ack = 1;
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
				/* check if we have atleast room for three packets (one ack,
				 * one forward request from user, one data packet from user) */
				if(packet_buffer_has_room_for_packets(&c->sq, 3)) {
					c->cb->neighbor_recv(c, &p->hdr.originator, &p->hdr.sender,
							p->hdr.hops, p->hdr.seqno, data, data_len);
					store_packet_for_dupe_checks(c, p);
				} else {
					LOG("Packet buffer overflowing. Dropping packet\n");
					send_ack = 0;
				}
			}

			if (send_ack) {
				/* Make ACK packet. */
				struct unicast_packet ap;
				init_unicast_packet(&ap, ACK, 0, &p->hdr.originator,
						&rimeaddr_node_addr, p->hdr.seqno, &p->hdr.sender);
				if (packet_buffer_find_buffered_packet(&c->sq,
							(struct packet*)&ap, unicast_packet_cmp) == NULL) {
					packet_buffer_unicast_packet(&c->sq, &ap, NULL, 0, NULL,
							MSG_TYPE_NEIGHBOR_ACK);
					ctimer_set(&c->neighbor_ack_timer,
							FAST_TRANSMIT_ACK, send_neighbor_ack, c);
				} else {
					LOG("Found ack in sending queue already\n");
				}
			}
		}
	} else {
		LOG("NOT FOR US OR NOT A NEIGHBOR: ");
		DEBUG_PACKET(p);
	}
}

static void broadcast_recv(struct abc_conn *bc) {
	const struct packet *p = (struct packet*)packetbuf_dataptr();
	struct ec *c = (struct ec*)((char*)bc-offsetof(struct ec, broadcast_conn));
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
		print_packet_data((uint8_t*)p->data, data_len);
		c->cb->broadcast_recv(c, &p->hdr.originator, &p->hdr.sender,
				p->hdr.hops, p->hdr.seqno, p->data, data_len);
		store_packet_for_dupe_checks(c, p);
	}
}

void ec_reliable_broadcast_ns(struct ec *c, const rimeaddr_t *originator, 
		const rimeaddr_t *sender, uint8_t hops, uint8_t seqno, const void *data,
		uint8_t data_len) {

	struct broadcast_packet bp;

	init_broadcast_packet(&bp, 0, hops, originator, sender, seqno);

	packet_buffer_broadcast_packet(&c->sq, &bp, data, data_len, c->ns,
			MSG_TYPE_NEIGHBOR_DATA);

	store_packet_for_dupe_checks(c, (struct packet*)&bp);

	if (ctimer_expired(&c->neighbor_data_timer)) {
		ctimer_set(&c->neighbor_data_timer,
				FAST_TRANSMIT, send_neighbor_data, c);
	}
}

void ec_broadcast(struct ec *c, const rimeaddr_t *originator, 
		const rimeaddr_t *sender, uint8_t hops, uint8_t seqno, const void *data,
		uint8_t data_len) {

	struct broadcast_packet bp;

	init_broadcast_packet(&bp, 0, hops, originator, sender, seqno);

	LOG("1\n");
	packet_buffer_broadcast_packet(&c->sq, &bp, data, data_len, c->ns,
			MSG_TYPE_BROADCAST_DATA);
	LOG("2\n");

	store_packet_for_dupe_checks(c, (struct packet*)&bp);

	if (ctimer_expired(&c->broadcast_data_timer)) {
		ctimer_set(&c->broadcast_data_timer,
				FAST_TRANSMIT, send_broadcast_data, c);
	}
}

void ec_mesh(struct ec *c, const rimeaddr_t *destination, uint8_t seqno, 
		const void *data, uint8_t data_len) {

	struct unicast_packet up;

	init_unicast_packet(&up, 0, 0, &rimeaddr_null,
			&rimeaddr_null, seqno, destination);

	packet_buffer_unicast_packet(&c->sq, &up, data, data_len, NULL,
			MSG_TYPE_MESH_DATA);

	if (ctimer_expired(&c->mesh_data_timer)) {
		ctimer_set(&c->mesh_data_timer,
				FAST_TRANSMIT, send_mesh_data, c);
	}
}

static void meshdata_recv(struct mesh_conn *bc, const rimeaddr_t *from, 
		uint8_t hops) {
	struct ec *c = (struct ec*)((char*)bc-offsetof(struct ec, meshdata_conn));
	const struct mesh_packet *mp = 
		(struct mesh_packet*)packetbuf_dataptr();

	uint8_t data_len = packetbuf_datalen() - MESH_PACKET_HDR_SIZE;
	TRACE("[MESH RECV]\n");
	print_packet_data((uint8_t*)mp, packetbuf_datalen());

	c->cb->mesh(c, from, hops, mp->seqno, mp->data, data_len);
}

static void meshdata_sent(struct mesh_conn *bc) {
	struct ec *c = (struct ec*)((char*)bc-offsetof(struct ec, meshdata_conn));
	struct buffered_packet *bp =
		packet_buffer_get_first_packet_from_type(&c->sq,
				MSG_TYPE_MESH_DATA);
	LOG("Mesh sent OK\n");
	packet_buffer_free(&c->sq, bp);

	ctimer_set(&c->mesh_data_timer,
			FAST_TRANSMIT, send_mesh_data, c);
}

static void meshdata_timeout(struct mesh_conn *bc) {
	struct ec *c = (struct ec*)((char*)bc-offsetof(struct ec, meshdata_conn));
	LOG("Mesh timed-out\n");
	ctimer_set(&c->mesh_data_timer,
			FAST_TRANSMIT, send_mesh_data, c);
}

static void timesynch_as_leader(void *ptr) {
	struct ec *c = (struct ec*)ptr;
	struct broadcast_packet bp;
	set_authority_level(timesynch_rimeaddr_to_authority(&rimeaddr_node_addr));
	set_authority_seqno(c->ts.seqno);

	init_broadcast_packet(&bp, TIMESYNCH, 0, &rimeaddr_node_addr,
			&rimeaddr_node_addr, c->ts.seqno++);

	LOG("[TIMESYNCH AS LEADER]: ");
	DEBUG_PACKET(&bp);

	packet_buffer_broadcast_packet(&c->sq, &bp, NULL, 0, NULL,
			MSG_TYPE_TIMESYNCH_DATA);

	ctimer_set(&c->timesynch_data_timer, FAST_TRANSMIT, send_timesynch_data, c);
	ctimer_set(&c->ts.timer, TIMESYNCH_LEADER_UPDATE, timesynch_as_leader, c);
	
	c->cb->timesynch(c);
}

static void timesynch_recv(struct abc_conn *bc) {
	struct ec *c = (struct ec*)((char*)bc-offsetof(struct ec, timesynch_conn));
	if (c->ts.is_on) {
		const struct packet *p = (struct packet*)packetbuf_dataptr();
		authority_t authlevel = timesynch_rimeaddr_to_authority(&p->hdr.originator);

		LOG("[TIMESYNCH RECV]: ");
		DEBUG_PACKET(p);

		ASSERT(IS_PACKET_FLAG_SET(p, TIMESYNCH));

		if (authlevel < authority_level) {
			struct broadcast_packet bp;
			authority_t my_authlevel = 
				timesynch_rimeaddr_to_authority(&rimeaddr_node_addr);

			if (my_authlevel < authlevel) {
				timesynch_as_leader(c);
			} else {
				set_authority_level(authlevel);
				set_authority_seqno(p->hdr.seqno);

				init_broadcast_packet(&bp, TIMESYNCH, p->hdr.hops+1, 
						&p->hdr.originator, &rimeaddr_node_addr, p->hdr.seqno);

				LOG("New leader. Forwarding timesynch packet: ");
				DEBUG_PACKET(&bp);

				packet_buffer_broadcast_packet(&c->sq, &bp, NULL, 0, NULL,
						MSG_TYPE_TIMESYNCH_DATA);

				ctimer_set(&c->timesynch_data_timer, FAST_TRANSMIT,
						send_timesynch_data, c);
				ctimer_set(&c->ts.timer, TIMESYNCH_LEADER_TIMEOUT, timesynch_as_leader, c);
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
				DEBUG_PACKET(&bp);

				packet_buffer_broadcast_packet(&c->sq, &bp, NULL, 0, NULL,
						MSG_TYPE_TIMESYNCH_DATA);

				ctimer_set(&c->timesynch_data_timer, FAST_TRANSMIT, send_timesynch_data, c);
				ctimer_set(&c->ts.timer, TIMESYNCH_LEADER_TIMEOUT, timesynch_as_leader, c);

				c->cb->timesynch(c);
			} else if (p->hdr.seqno == authority_seqno) {
				c->cb->timesynch(c);
			}
		}
	}
}


static const struct abc_callbacks neighbor_cb = {neighbor_recv};
static const struct abc_callbacks timesynch_cb = {timesynch_recv};
static const struct abc_callbacks broadcast_cb = {broadcast_recv};
static const struct mesh_callbacks meshdata_cb = {meshdata_recv, meshdata_sent,
	meshdata_timeout};

void ec_open(struct ec *c, uint16_t data_channel, 
		const struct ec_callbacks *cb) {

	timesynch_init();

	abc_open(&c->neighbor_conn, data_channel, &neighbor_cb);
	abc_open(&c->timesynch_conn, data_channel+1, &timesynch_cb);
	set_timesynch_channel(data_channel+1);

	abc_open(&c->broadcast_conn, data_channel+2, &broadcast_cb);
	mesh_open(&c->meshdata_conn, data_channel+3, &meshdata_cb);

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
	mesh_close(&c->meshdata_conn);
}

void ec_set_neighbors(struct ec *c, const struct neighbors *ns) {
	c->ns = ns;
}

void ec_timesynch_network(struct ec *c) {
	ASSERT(c->ts.is_on == 1);
	if(ctimer_expired(&c->ts.timer)) {
		timesynch_as_leader(c);
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
