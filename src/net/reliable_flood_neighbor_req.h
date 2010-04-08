/**
 * TEAM LK
 * CN3
 *
 * RELIABLE FLOOD w/ NEIGHBOR REQUIREMENT
 *
 * This implementation takes care of the reliable flooding of packets in the
 * network by using ACKs and resend timers; and have a neighbor-to-neighbor
 * requirement set on the packets recieved. The requirement is set so that
 * only packets from close neighors are passed to the application level (the
 * definiton of a close neighbor is based on signal strength and is implemented
 * in neighbors.c). If a packet is recieved but not classified as coming from a
 * neighbor, it is discarded i.e. neither sent to the application level nor
 * forwarded.
 *
 * TODO(johan):There is a fundamental flaw in the design and that is that the
 * first packets, before the neighbor-classifier has gotten enough data to make
 * good classifications, are always passed to the application level. A way
 * around this would be to use neighbor discovery broadcasts every now and then.
 * Sucks some batteries though :S.
 */

#ifndef _RELIABLE_FLOOD_WITH_NEIGHBOR_REQUIREMENT_
#define _RELIABLE_FLOOD_WITH_NEIGHBOR_REQUIREMENT_

#include "net/rime/abc.h"
#include "net/rime/ctimer.h"
#include "net/rime/rimeaddr.h"

#include "base/queue_buffer.h"

#include "net/packet.h"
#include "net/packet_buffer.h"
#include "net/neighbors.h"

#define DUPE_CHECK_PACKETS_BUF_LENGTH 8
#define UNACKED_PACKETS_BUF_LENGTH 8

typedef enum {
	FORWARD_PACKET = 0x01, 
	DROP_PACKET = 0x02,

	/* The connection makes it possible to store ONE packet for further
	 * injection. The injection can be done many times, the packet is not
	 * discarded when forwarded from a stored state. */
	/* TODO(johan): move this up to application level? storing just one packet is
	 * kinda a restriction, however works for the time being. */
	STORE_PACKET_FOR_FORWARDING = 0x04,
	FORWARD_STORED_PACKET = 0x08,

	/* The connection makes it possible to store DUPE_CHECK_PACKETS_BUF_LENGTH
	 * packets for future dupe checks. The user is notified of a dupe packet by
	 * the rffcallbacks.dupe_recv function. */
	STORE_PACKET_FOR_DUPE_CHECKS= 0x10
}rfnr_instructions_t;


struct rfnr_conn;
typedef rfnr_instructions_t (*rfnr_callback_t)(struct rfnr_conn *c, 
			const rimeaddr_t *originator, const rimeaddr_t *forwarder,
			uint8_t hops, const void *data, uint8_t data_len);

struct rfnr_callbacks {
	rfnr_callback_t recv;
	rfnr_callback_t dupe_recv;
};

struct rfnr_conn {
	struct abc_conn data_conn;
	struct abc_conn ack_conn;
	PACKET_BUFFER(unacked, UNACKED_PACKETS_BUF_LENGTH);
	struct buffered_packet *next_to_send;

	/* Packets stored from flag STORE_PACKET_FOR_DUPE_CHECKS */
	QUEUE_BUFFER(dupe, SLIM_PACKET_SIZE, DUPE_CHECK_PACKETS_BUF_LENGTH);

	struct neighbors ns;

	/* Packet stored from flag STORE_PACKET_FOR_FORWARDING */
	struct {
		uint8_t initialized;
		uint8_t data_len;
		struct packet p;
		uint8_t data[MAX_DATA_SIZE];
	}stored_packet;

	uint8_t seqno;
	struct ctimer send_timer;
	const struct rfnr_callbacks *cb;
};


void rfnr_open(struct rfnr_conn *c, uint16_t data_channel, 
		uint16_t ack_channel, const struct rfnr_callbacks *cb);

void rfnr_close(struct rfnr_conn *c);

void rfnr_async_send(struct rfnr_conn *c, const void *data, uint8_t data_len);

/**
 * Checks if naddr is a neighbor. Returns 1 if true. 0 Otherwise.
 *
 * XXX(johan): used currently by the application layer to protect against early
 * neighbor classification errors.
 */
int rfnr_is_neighbor(struct rfnr_conn *c, const rimeaddr_t *naddr);
#endif
