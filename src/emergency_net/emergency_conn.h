/* Emergency connection provides a neighbor requirement on all communication
 * inbetween nodes. */
#ifndef _EMERGENCY_CONN_H_
#define _EMERGENCY_CONN_H_

#include "net/rime/abc.h"
#include "net/rime/mesh.h"
#include "net/rime/ctimer.h"
#include "net/rime/rimeaddr.h"

#include "base/queue_buffer.h"

#include "emergency_net/packet_buffer.h"
#include "emergency_net/neighbors.h"

#define SENDING_QUEUE_LENGTH 12
#define DUPE_QUEUE_LENGTH 24

struct ec;
typedef void (*ec_callback_data_t)(struct ec *c, 
			const rimeaddr_t *originator, const rimeaddr_t *sender,
			uint8_t hops, uint8_t seqno, const void *data, uint8_t data_len);
typedef void (*ec_callback_timesynch_t)(struct ec *c);	
typedef void (*ec_callback_mesh_t)(struct ec *c, const rimeaddr_t *originator,
		uint8_t hops, uint8_t seqno, const void *data, uint8_t data_len);

struct ec_callbacks {
	ec_callback_data_t broadcast_recv;
	ec_callback_data_t multicast_unicast_recv;
	ec_callback_data_t neighbor_recv;
	ec_callback_timesynch_t timesynch;
	ec_callback_mesh_t mesh;
};

struct ec {
	struct abc_conn neighbor_conn;
	struct abc_conn timesynch_conn;
	struct abc_conn broadcast_conn;
	struct mesh_conn meshdata_conn;

	struct ctimer neighbor_ack_timer;
	struct ctimer neighbor_data_timer;
	struct ctimer broadcast_data_timer;
	struct ctimer multicast_unicast_data_timer;
	struct ctimer multicast_unicast_ack_timer;
	struct ctimer timesynch_data_timer;
	struct ctimer mesh_data_timer;

	PACKET_BUFFER(sq, SENDING_QUEUE_LENGTH);

	QUEUE_BUFFER(dq, SLIM_PACKET_SIZE, DUPE_QUEUE_LENGTH);

	const struct neighbors *ns;

	struct {
		struct ctimer timer;
		uint8_t seqno;
		int8_t is_on;
	} ts; /* time synch */

	const struct ec_callbacks *cb;
};

void ec_open(struct ec *c, uint16_t data_channel, 
		const struct ec_callbacks *cb);

void ec_close(struct ec *c);

void ec_set_neighbors(struct ec *c, const struct neighbors *ns);

/* reliable in-order broadcast to neighbors */
void ec_reliable_broadcast_ns(struct ec *c, const rimeaddr_t *originator, 
		const rimeaddr_t *sender, uint8_t hops, uint8_t seqno, const void *data,
		uint8_t data_len);

/* best effort broadcast to everyone */
void ec_broadcast(struct ec *c, const rimeaddr_t *originator, 
		const rimeaddr_t *sender, uint8_t hops, uint8_t seqno, const void *data,
		uint8_t data_len);

void ec_reliable_multicast(struct ec *c, const struct neighbors *receivers, const
		rimeaddr_t *originator, const rimeaddr_t *sender, uint8_t hops, uint8_t
		seqno, const void *data, uint8_t data_len);

void ec_reliable_unicast(struct ec *c, const rimeaddr_t *destination, const rimeaddr_t
		*originator, const rimeaddr_t *sender, uint8_t hops, uint8_t seqno,
		const void *data, uint8_t data_len);

void ec_mesh(struct ec *c, const rimeaddr_t *destination,
		uint8_t seqno, const void *data, uint8_t data_len);

void ec_timesynch_on(struct ec *c);
void ec_timesynch_off(struct ec *c);
void ec_timesynch_network(struct ec *c);
#endif
