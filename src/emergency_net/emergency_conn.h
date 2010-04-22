#ifndef _EMERGENCY_CONN_H_
#define _EMERGENCY_CONN_H_

#include "net/rime/abc.h"
#include "net/rime/ctimer.h"
#include "net/rime/rimeaddr.h"

#include "emergency_net/packet_buffer.h"
#include "base/queue_buffer.h"

#include "emergency_net/neighbors.h"

#define SENDING_QUEUE_LENGTH 8
#define DUPE_QUEUE_LENGTH 16

struct ec;
typedef void (*ec_callback_t)(struct ec *c, 
			const rimeaddr_t *originator, const rimeaddr_t *sender,
			uint8_t hops, uint8_t seqno, const void *data, uint8_t data_len);

struct ec_callbacks {
	ec_callback_t recv;
	ec_callback_t recv_dupe;
	//ec_callback_t mesh_timeout;
};

struct ec {
	struct abc_conn data_conn;
	struct abc_conn ack_conn;
	PACKET_BUFFER(sq, SENDING_QUEUE_LENGTH);
	struct buffered_packet *next_to_send;

	QUEUE_BUFFER(dq, SLIM_PACKET_SIZE, DUPE_QUEUE_LENGTH);

	struct neighbors ns;

	struct ctimer send_timer;
	const struct ec_callbacks *cb;
};

void ec_open(struct ec *c, uint16_t data_channel, 
		const struct ec_callbacks *cb);

void ec_close(struct ec *c);

void ec_async_broadcast(struct ec *c, const rimeaddr_t *originator, 
		const rimeaddr_t *sender, uint8_t hops, uint8_t seqno, const void *data,
		uint8_t data_len);

void ec_async_multicast(struct ec *c, uint8_t number_of_destinations, 
		const rimeaddr_t *destinations, const void *data, uint8_t data_len);

void ec_async_unicast(struct ec *c, const rimeaddr_t *destination, 
		const void *data, uint8_t data_len);

//void ec_async_mesh(struct ec *c, const rimeaddr_t *destination, 
//		const void *data, uint8_t data_len);

void ec_update_neighbors(struct ec *c, const struct neighbors *ns);

//void ec_update_route_table(struct ec_route_table *c);

#endif
