#include "contiki.h"

#include "dev/leds.h"
#include "dev/button-sensor.h"
#include "dev/serial-line.h"
#include "dev/light-sensor.h"

#include "dev/sky-sensors.h"

#include "sys/rtimer.h"
#include "net/rime/timesynch.h"

#include "string.h"
//#include "limits.h"

#include "emergency_net/emergency_conn.h"
#include "emergency_net/neighbors.h"

#include "base/node_properties.h"

#include "base/util.h"
#include "base/log.h"

/* 0 means no simulation */
#define EMERGENCY_COOJA_SIMULATION 2

#define EMERGENCYNET_CHANNEL 128
#define BLINKING_SEQUENCE_LENGTH 8
#define BLINKING_SEQUENCE_SWITCH_TIME (4*1024)
#define MAX_FIRE_COORDINATES 10

#define MAX_ALLOWED_METRIC 1000
#define LIGHT_EMERGENCY_THRESHOLD 400
#define ABRUPT_METRIC_CHANGE_THRESHOLD 200


/****** PACKET TYPES ******/
enum {
	/* System in action */

	/* Sent when we have sensed emergency (e.g. fire). This is broadcasted without
	 * neighbor req, and flooded throughout the network. */
	EMERGENCY_PACKET,
	ANTI_EMERGENCY_PACKET,

	/* Sent when we have calculated a new best path. */
	BEST_PATH_UPDATE_PACKET,

	/* GUI setup packet. Can be sent to the node when the usr button has been
	 * pushed. Is used for uploading rimeaddr, coord, if it is an exit node, and
	 * the neighbors of the node. */
	SETUP_PACKET,

	/* This is sent to tell the sensors to begin exchanging information about
	 * nearest paths, coordinates etc. See NODE_INFO_PACKET. */
	INITIALIZE_BEST_PATHS_PACKET,

	/* Sent when INITIALIZE_BEST_PATHS_PACKET is sent for exit nodes and sent
	 * when regular nodes get NODE_INFO_PACKET from other nodes (not only exit
	 * nodes). This to be able to calculate nearest paths (we get coordinates
	 * of neighbors from this packet) and store them so we can start blinking
	 * immediately and do not need to figure out best paths etc at that time.
	 * This packet is only sent once to the node's neighbors. */
	NODE_INFO_PACKET,

	EXTRACT_REPORT_PACKET,
	NODE_REPORT_PACKET,

	RESET_SYSTEM_PACKET
};


struct sensor_packet {
	uint8_t type;
};

struct emergency_packet {
	uint8_t type;
	struct coordinate source; /* of emergency */
};

struct best_path_update_packet {
	uint8_t type;
	struct neighbor_node_best_path bp;
};

#define SETUP_PACKET_SIZE (sizeof(struct setup_packet)-sizeof(rimeaddr_t))
struct setup_packet {
	uint8_t type;
	rimeaddr_t new_addr;
	struct coordinate new_coord;
	int8_t is_exit_node;
	uint8_t num_neighbors;
	rimeaddr_t neighbors[1];
};

struct node_info_packet {
	uint8_t type;
	struct coordinate coord;
	struct neighbor_node_best_path bp;
};

struct node_report_packet {
	uint8_t type;
	struct coordinate coord;
	int8_t is_burning;
	int8_t is_exit_node;
};


enum blinking_sequence_state {
	ON = 0, OFF_1 = 1, OFF_2 = 2, OFF_3 = 3 , OFF_4 = 4, OFF_5 = 5, OFF_6 = 6,
	OFF_7 = 7
};

struct sensor_readings {
	int light;
	/*int temp;
	int humidity;*/
};

struct node_properties {
	struct neighbors ns;
	const struct neighbor_node *bpn; /* best path neighbor */

	QUEUE_BUFFER(emergency_coords, 
			sizeof(struct coordinate), MAX_FIRE_COORDINATES);

	struct {
		struct rtimer rt;
		rtimer_clock_t next_wakeup;
		enum blinking_sequence_state state;
	} blinking;

	struct {
		int8_t is_blinking;
		int8_t is_burning;
		int8_t is_exit_node;
		int8_t is_awaiting_setup_packet;
		int8_t has_sent_node_info;
		int8_t is_reset_mode;
		int8_t is_sink_node;
	} state;

	uint8_t current_sensors_metric[2];

	struct ec c;
	uint8_t seqno;
};

/* GLOBAL VARIABLES */

static struct node_properties g_np;

/* This is only used when this node is infact an exit node. We could just add
 * ourselves to g_np.ns list but that would screw up the emergency_conn, trying
 * to send to ourselves is not such a good idea.*/
static struct neighbor_node exit_node;

/* FUNCTIONS */

static void 
add_coordinate_as_burning(const struct coordinate *coord) {
	struct coordinate *i = 
		(struct coordinate*)queue_buffer_find(&g_np.emergency_coords, coord,
				coordinate_cmp);
	if(i == NULL) {
		queue_buffer_push_front(&g_np.emergency_coords, coord);
	}
}

static void 
remove_coordinate_as_burning(const struct coordinate *coord) {
	struct coordinate *i = queue_buffer_find( &g_np.emergency_coords, coord,
			coordinate_cmp);
	if(i != NULL) {
		queue_buffer_free(&g_np.emergency_coords, i);
	}
}

static void
send_emergency_packet() {
	struct emergency_packet ep;
	ep.type = EMERGENCY_PACKET;
	coordinate_copy(&ep.source, &coordinate_node);
	ec_broadcast(&g_np.c, &rimeaddr_node_addr,
			&rimeaddr_node_addr, 0, g_np.seqno++, &ep,
			sizeof(struct emergency_packet));
}

static void
send_anti_emergency_packet() {
	struct emergency_packet ep;
	ep.type = ANTI_EMERGENCY_PACKET;
	coordinate_copy(&ep.source, &coordinate_node);
	ec_broadcast(&g_np.c, &rimeaddr_node_addr,
			&rimeaddr_node_addr, 0, g_np.seqno++, &ep,
			sizeof(struct emergency_packet));
}


static void 
blink(struct rtimer *rt, void *ptr) {
	if (g_np.state.is_blinking && !g_np.state.is_burning) {
		ASSERT(g_np.bpn != NULL);
		if (neighbor_node_metric(g_np.bpn) < MAX_ALLOWED_METRIC) {
			switch(g_np.blinking.state) {
				case ON:
					leds_blue(1);
					break;
				case OFF_1:
				case OFF_2:
				case OFF_3:
				case OFF_4:
				case OFF_5:
				case OFF_6:
				case OFF_7:
					leds_blue(0);
					break;
			}
		} else {
			leds_blue(1);
		}

		g_np.blinking.next_wakeup += BLINKING_SEQUENCE_SWITCH_TIME;
		rtimer_set(&g_np.blinking.rt, g_np.blinking.next_wakeup, 0, blink, NULL);

		g_np.blinking.state = (g_np.blinking.state+1) % BLINKING_SEQUENCE_LENGTH;
	}
}

static void
blinking_update() {
	if (g_np.state.is_blinking && !g_np.state.is_burning) {
		uint16_t blinking_sequences_next = (timesynch_time() /
				BLINKING_SEQUENCE_SWITCH_TIME)+1;
		g_np.blinking.next_wakeup =
			timesynch_time_to_rtimer(blinking_sequences_next*
					BLINKING_SEQUENCE_SWITCH_TIME);

		rtimer_set(&g_np.blinking.rt, g_np.blinking.next_wakeup, 0, blink,
				NULL);

		ASSERT(g_np.bpn != NULL);
		g_np.blinking.state =
			(neighbor_node_hops(g_np.bpn)+1+blinking_sequences_next) %
			BLINKING_SEQUENCE_LENGTH;
	} 
}

static void
blinking_init() {
	if (!g_np.state.is_blinking) {
		g_np.state.is_blinking = 1;
		blinking_update();
	}
}

static void
set_burning(int8_t on) {
	g_np.state.is_burning = on;
	leds_red(on);
	leds_blue(0);
}

void setup_parse(const struct setup_packet *sp, int is_from_flash) {
	int i = 0;
	const rimeaddr_t *addr = sp->neighbors;

	leds_off(LEDS_ALL);

	rimeaddr_set_node_addr((rimeaddr_t*)&sp->new_addr);

	coordinate_set_node_coord(&sp->new_coord);

	neighbors_clear(&g_np.ns);
	memset(&g_np.state, 0, sizeof(g_np.state));

	uint16_to_uint8(0, g_np.current_sensors_metric);

	for(; i < sp->num_neighbors; ++i) {
		neighbors_add(&g_np.ns, addr++);
	}
	
	if (sp->is_exit_node) {
		struct neighbor_node_best_path bp;
		bp.metric[0] = 0;
		bp.metric[1] = 0;
		rimeaddr_copy(&bp.points_to, &rimeaddr_node_addr);
		bp.hops = -1; /* since we calculate +1 when sending path */

		g_np.state.is_exit_node = 1;
		neighbor_node_set_addr(&exit_node, &rimeaddr_node_addr);
		neighbor_node_set_coordinate(&exit_node, &coordinate_node);
		neighbor_node_set_best_path(&exit_node, &bp);

		leds_green(1);
	}

	LOG("Id: %d.%d, Coord: (%d.%d, %d.%d), is_exit_node: %d\n", rimeaddr_node_addr.u8[0],
			rimeaddr_node_addr.u8[1], 
			coordinate_node.x[0], 
			coordinate_node.x[1], 
			coordinate_node.y[0], 
			coordinate_node.y[1],
			g_np.state.is_exit_node);
	LOG("Neighbors:\n");
	{
		const struct neighbor_node *nn = neighbors_begin(&g_np.ns);
		for (; nn != NULL;
				nn = neighbors_next(&g_np.ns)) {
			LOG("neighbor: %d.%d, points_to: (%d.%d), coord: (%d.%d,%d.%d), distance: %d, "
					"hops: %d, metric: %u\n",
					neighbor_node_addr(nn)->u8[0], 
					neighbor_node_addr(nn)->u8[1],
					nn->bp.points_to.u8[0],
					nn->bp.points_to.u8[1],
					neighbor_node_coord(nn)->x[0], 
					neighbor_node_coord(nn)->x[1], 
					neighbor_node_coord(nn)->y[0],
					neighbor_node_coord(nn)->y[1],
					neighbor_node_distance(nn),
					neighbor_node_hops(nn),
					neighbor_node_metric(nn));
		}
	}

//#ifndef EMERGENCY_COOJA_SIMULATION
	if(!is_from_flash) {
		LOG("Burning info to flash\n");
		node_properties_burn(sp, SETUP_PACKET_SIZE + 
				sp->num_neighbors * sizeof(rimeaddr_t));
		LOG("Burning info to flash OK\n");
	}
//#endif
	ec_set_neighbors(&g_np.c, &g_np.ns);
}

static inline metric_t 
weigh_distance(distance_t distance) {
	return distance;
}

static inline metric_t
weigh_metrics(distance_t distance) {
	metric_t sens_16;
	uint8_to_uint16(g_np.current_sensors_metric, &sens_16);
	if (g_np.state.is_burning) {
		return 1000+weigh_distance(distance)+sens_16;
	}

	return weigh_distance(distance);
}


static const struct neighbor_node*
find_best_exit_path_neighbor() {
	const struct neighbor_node *best = NULL;
	const struct neighbor_node *i = neighbors_begin(&g_np.ns);
	metric_t min_metric = METRIC_T_MAX;
	metric_t metric;

	LOG("-----------------------\n");
	if (!g_np.state.is_exit_node) {
		for(;i != NULL; i = neighbors_next(&g_np.ns)) {
			TRACE("neighbor: %d.%d, points_to: (%d.%d), coord: (%d.%d,%d.%d), distance: %d, "
					"hops: %d, metric: %u\n",
					neighbor_node_addr(i)->u8[0], 
					neighbor_node_addr(i)->u8[1],
					i->bp.points_to.u8[0],
					i->bp.points_to.u8[1],
					neighbor_node_coord(i)->x[0], 
					neighbor_node_coord(i)->x[1], 
					neighbor_node_coord(i)->y[0],
					neighbor_node_coord(i)->y[1],
					neighbor_node_distance(i),
					neighbor_node_hops(i),
					neighbor_node_metric(i));

			if(!neighbor_node_points_to_us(i)) { 
				metric = weigh_distance( neighbor_node_distance(i) ) +
					neighbor_node_metric(i);
				if (metric < min_metric) {
					min_metric = metric;
					best = i;
				}
			}
		}
	} else {
		/* extra checks for exit nodes */
	//	uint16_t metric16;
	//	uint8_to_uint16(g_np.current_sensors_metric, &metric16);
	//	if (metric16 < min_metric) {
			best = &exit_node;
		//}
	}

	ASSERT(best != NULL);
	TRACE("BEST neighbor: %d.%d, points_to_us: %d, coord: (%d.%d,%d.%d), distance: %d, "
			"metric: %d\n",
			neighbor_node_addr(best)->u8[0], neighbor_node_addr(best)->u8[1],
			neighbor_node_points_to_us(best),
			neighbor_node_coord(best)->x[0], 
			neighbor_node_coord(best)->x[1], 
			neighbor_node_coord(best)->y[0],
			neighbor_node_coord(best)->y[1],
			neighbor_node_distance(best),
			neighbor_node_metric(best));
	LOG("-----------------------\n");

	return best;
}

/* Transforms our best neighbor's best path to our own. */
void best_neighbor_bp_to_our_bp(struct neighbor_node_best_path *bp) {
	ASSERT(g_np.bpn != NULL);
	uint16_to_uint8(
			neighbor_node_metric(g_np.bpn) +
			weigh_metrics(neighbor_node_distance(g_np.bpn)),
			bp->metric
			);

	rimeaddr_copy(&bp->points_to, neighbor_node_addr(g_np.bpn));
	bp->hops = neighbor_node_hops(g_np.bpn) + 1;
}

static void
broadcast_best_path() {
	struct best_path_update_packet bpup;

	bpup.type = BEST_PATH_UPDATE_PACKET;
	best_neighbor_bp_to_our_bp(&bpup.bp);

	LOG("SENDING BEST_PATH_UPDATE_PACKET: metric: [%d %d], points_to: "
			"%d.%d, hops: %d\n",
			bpup.bp.metric[0], 
			bpup.bp.metric[1], 
			bpup.bp.points_to.u8[0],
			bpup.bp.points_to.u8[1], 
			bpup.bp.hops);

	ec_reliable_broadcast_ns(&g_np.c, &rimeaddr_node_addr, &rimeaddr_node_addr,
			0, g_np.seqno++, &bpup, sizeof(struct best_path_update_packet));
}

static int
update_bpn_and_broadcast_new_path_if_changed(const struct neighbor_node *sender) {
	const struct neighbor_node *best = find_best_exit_path_neighbor();
	ASSERT(best != NULL);

	if (g_np.bpn == NULL || 
			neighbor_node_metric(best) < neighbor_node_metric(g_np.bpn)) {
		/* Neighbor now has the new best path */
		g_np.bpn = best;
		broadcast_best_path();
		return 1;
	} else if (sender == best) {
		/* Neighbor updated its metrics. Broadcast changes. */
		g_np.bpn = best;
		broadcast_best_path();
		return 1;
	}

	return 0;
}

static void update_bpn_and_send_node_info() {
	struct node_info_packet nip;
	const struct neighbor_node *best =
		find_best_exit_path_neighbor();
	ASSERT(best != NULL);
	g_np.bpn = best;

	nip.type = NODE_INFO_PACKET;
	coordinate_copy(&nip.coord, &coordinate_node);
	best_neighbor_bp_to_our_bp(&nip.bp);

	LOG("SENDING NODE_INFO_PACKET: coord: (%d.%d,%d.%d), metric: %d,%d, "
			"points_to: %d.%d, hops: %d\n",
			nip.coord.x[0], 
			nip.coord.x[1], 
			nip.coord.y[0], 
			nip.coord.y[1], 
			nip.bp.metric[0], 
			nip.bp.metric[1], 
			nip.bp.points_to.u8[0],
			nip.bp.points_to.u8[1],
			nip.bp.hops);

	ec_reliable_broadcast_ns(&g_np.c,
			&rimeaddr_node_addr, &rimeaddr_node_addr, 0,
			g_np.seqno++, &nip, sizeof(struct node_info_packet));
}

static void initialize_best_path_packet_handler() {
	ec_timesynch_on(&g_np.c);

	if (g_np.state.is_reset_mode) {
		g_np.state.is_reset_mode = 0;
		leds_off(LEDS_ALL);
		if(g_np.state.is_exit_node) {
			leds_green(1);
		}
	}
	if(g_np.state.is_exit_node) {
		ASSERT(!g_np.state.has_sent_node_info);
		update_bpn_and_send_node_info();
		g_np.state.has_sent_node_info = 1;
	}
}

static void reset_system_packet_handler() {
	struct neighbor_node *nn = 
		(struct neighbor_node*)neighbors_begin(&g_np.ns);
	leds_on(LEDS_ALL);

	for(; nn != NULL; nn = (struct neighbor_node*)neighbors_next(&g_np.ns)) {
		neighbor_node_set_best_path(nn, &neighbor_node_best_path_max);
		neighbor_node_set_coordinate(nn, &coordinate_null);
	}

	g_np.bpn = NULL;
	g_np.state.is_blinking = 0;
	g_np.state.is_burning = 0;
	g_np.state.is_awaiting_setup_packet = 0;
	/*g_np.state.is_exit_node = 0;*/
	g_np.state.has_sent_node_info = 0;
	g_np.state.is_reset_mode = 1;
	g_np.state.is_sink_node = 0;


	queue_buffer_clear(&g_np.emergency_coords);
	uint16_to_uint8(0, g_np.current_sensors_metric);

	ec_timesynch_off(&g_np.c);

}

/* Received packets from everyone (whose packets are sent with
 * ec_async_broadcast) */
static void ec_broadcasts_recv(struct ec *c, const rimeaddr_t *originator, 
		const rimeaddr_t *sender, uint8_t hops, uint8_t seqno, 
		const void *data, uint8_t data_len) {
	const struct sensor_packet *p = (struct sensor_packet*)data;

	/* ignore every other packet if in reset mode */
	if(g_np.state.is_reset_mode) {
		if(p->type != INITIALIZE_BEST_PATHS_PACKET) {
			return;
		}
	}

	if (!g_np.state.is_sink_node) {
		switch(p->type) {
			case EMERGENCY_PACKET:
				{
					struct emergency_packet *ep = (struct emergency_packet*)p;
					LOG("RECV EMERGENCY_PACKET: coord: [%d%d],[%d%d]\n",
							ep->source.x[0], ep->source.x[1], ep->source.y[0], ep->source.y[1]);
					add_coordinate_as_burning(&ep->source);

					/* forward packet */
					ec_broadcast(&g_np.c, originator, &rimeaddr_node_addr,
							hops+1, seqno, data, data_len);

					blinking_init();
				}
				break;
			case ANTI_EMERGENCY_PACKET:
				{
					struct emergency_packet *ep = (struct emergency_packet*)p;
					remove_coordinate_as_burning(&ep->source);

					LOG("RECV ANTI EMERGENCY_PACKET: coord: [%d%d],[%d%d]\n",
							ep->source.x[0], ep->source.x[1], ep->source.y[0], ep->source.y[1]);
					ec_broadcast(&g_np.c, originator, &rimeaddr_node_addr,
							hops+1, seqno, data, data_len);
				}
				break;
			case SETUP_PACKET:
				LOG("RECV SETUP_PACKET\n");
				if (g_np.state.is_awaiting_setup_packet) {
					g_np.state.is_awaiting_setup_packet = 0;
					setup_parse((const struct setup_packet*)p, 0);
					leds_blink();
				}
				break;
			case INITIALIZE_BEST_PATHS_PACKET:
				LOG("RECV INITIALIZE_BEST_PATHS_PACKET\n");
				ec_reliable_broadcast_ns(&g_np.c,
						originator, &rimeaddr_node_addr, hops+1,
						seqno, data, data_len);

				initialize_best_path_packet_handler();
				break;
			case EXTRACT_REPORT_PACKET:
				{
					struct node_report_packet nrp;
					LOG("RECV EXTRACT_REPORT_PACKET\n");
					nrp.type = NODE_REPORT_PACKET;
					coordinate_copy(&nrp.coord, &coordinate_node);
					nrp.is_burning = g_np.state.is_burning;
					nrp.is_exit_node = g_np.state.is_exit_node;

					ec_broadcast(&g_np.c, originator, &rimeaddr_node_addr,
							hops+1, seqno, data, data_len);

					ec_mesh(&g_np.c, originator, g_np.seqno++, &nrp, sizeof(struct
								node_report_packet));
				}
				break;
			case RESET_SYSTEM_PACKET:
				LOG("RECV RESET_SYSTEM_PACKET\n");
				ec_reliable_broadcast_ns(&g_np.c,
						originator, &rimeaddr_node_addr, hops+1,
						seqno, data, data_len);
				reset_system_packet_handler();
				break;
			default:
				LOG("Unknown application packet type\n");
				ASSERT(0);
		}
	} else {
		switch(p->type) {
			case EMERGENCY_PACKET:
				{
					//struct emergency_packet *ep = (struct emergency_packet*)p;
					printf("@EMERGENCY_PACKET:%d.%d\n", 
							originator->u8[0],
							originator->u8[1]);
				}
				break;
			case ANTI_EMERGENCY_PACKET:
				{
					printf("@ANTI_EMERGENCY_PACKET:%d.%d\n", 
							originator->u8[0],
							originator->u8[1]);
				}
				break;
			case SETUP_PACKET:
				break;
			case INITIALIZE_BEST_PATHS_PACKET:
				break;
			case EXTRACT_REPORT_PACKET:
				break;
			case RESET_SYSTEM_PACKET:
				break;
			default:
				LOG("Unknown application packet type\n");
				ASSERT(0);
		}
	}
}

static void
print_packet_data(const uint8_t *hdr, int len)
{
  int i;

  for(i = 0; i < len; ++i) {
    LOG(" (0x%0x), ", hdr[i]);
  }

  LOG("\n");
}

/* Received packets from neighbors only */
static void ec_neighbors_recv(struct ec *c, const rimeaddr_t *originator, 
		const rimeaddr_t *sender, uint8_t hops, uint8_t seqno, 
		const void *data, uint8_t data_len) {
	const struct sensor_packet *p = (struct sensor_packet*)data;

	/* ignore every other packet if in reset mode */
	if(g_np.state.is_reset_mode) {
		if(p->type != INITIALIZE_BEST_PATHS_PACKET) {
			return;
		}
	}

	if (g_np.state.is_sink_node) {
		return;
	}

	switch(p->type) {
		case BEST_PATH_UPDATE_PACKET:
			{
				const struct best_path_update_packet *bpup = 
					(struct best_path_update_packet*)p;
				struct neighbor_node *nn =
					neighbors_find_neighbor_node(&g_np.ns, sender);
				LOG("RECV BEST_PATH_UPDATE_PACKET: metric: %d,%d, points_to: "
						"%d.%d, hops: %d\n",
						bpup->bp.metric[0], 
						bpup->bp.metric[1], 
						bpup->bp.points_to.u8[0],
						bpup->bp.points_to.u8[1], 
						bpup->bp.hops);
				ASSERT(nn != NULL);

				neighbor_node_set_best_path(nn, &bpup->bp);

				/* The shortest path could have changed. */
				if(update_bpn_and_broadcast_new_path_if_changed(nn)) {
					blinking_update();
				}
			}
			break;
		case INITIALIZE_BEST_PATHS_PACKET:
			LOG("RECV INITIALIZE_BEST_PATHS_PACKET\n");
			ec_reliable_broadcast_ns(&g_np.c,
					originator, &rimeaddr_node_addr, hops+1,
					seqno, data, data_len);

			initialize_best_path_packet_handler();
			break;
		case NODE_INFO_PACKET:
			{
				const struct node_info_packet *nip = (struct node_info_packet*)p;
				struct neighbor_node *nn = 
					neighbors_find_neighbor_node(&g_np.ns, sender);
				//coord_aligned_to_coord(&nip->coord, &coord);
				//best_path_aligned_to_neighbor_node_best_path(&nip->bpa, &bp);
				LOG("RECV NODE_INFO_PACKET: coord: (%d.%d,%d.%d), metric: %d,%d, "
						"points_to: %d.%d, hops: %d\n",
						nip->coord.x[0], 
						nip->coord.x[1], 
						nip->coord.y[0], 
						nip->coord.y[1], 
						nip->bp.metric[0], 
						nip->bp.metric[1], 
						nip->bp.points_to.u8[0],
						nip->bp.points_to.u8[1],
						nip->bp.hops);
				print_packet_data((uint8_t*)nip, sizeof(struct node_info_packet));
				ASSERT(nn != NULL);
				neighbor_node_set_coordinate(nn, &nip->coord);
				neighbor_node_set_best_path(nn, &nip->bp);

				if (!g_np.state.has_sent_node_info) {
					update_bpn_and_send_node_info();
					g_np.state.has_sent_node_info = 1;
				} else {
					/* send update only if we found a better path */
					if (!g_np.state.is_exit_node) {
						update_bpn_and_broadcast_new_path_if_changed(nn);
					}
				}
			}
			break;
		case RESET_SYSTEM_PACKET:
			LOG("RECV RESET_SYSTEM_PACKET\n");
			ec_reliable_broadcast_ns(&g_np.c,
					originator, &rimeaddr_node_addr, hops+1,
					seqno, data, data_len);
			reset_system_packet_handler();
			break;
		default:
			LOG("Unknown application packet type\n");
			ASSERT(0);
	}
}

static void
ec_timesynch_recv(struct ec *c) {
	/* System got timesynched */
	blinking_update();
}

static void ec_mesh_recv(struct ec *c, const rimeaddr_t *originator,
		uint8_t hops, uint8_t seqno, const void *data, uint8_t data_len) {
	const struct sensor_packet *p = (struct sensor_packet*)data;

	switch(p->type) {
		case NODE_REPORT_PACKET:
			{
				const struct node_report_packet *nrp = 
					(struct node_report_packet*)p;
				uint16_t x;
				uint16_t y;
				uint8_to_uint16(nrp->coord.x, &x);
				uint8_to_uint16(nrp->coord.y, &y);
				LOG("RECV NODE_REPORT_PACKET\n");

				/* GUI SHOULD PARSE THIS. */
				printf("@NODE_REPORT_PACKET:%d.%d:%d.%d:%d:%d\n",
						originator->u8[0],
						originator->u8[1],
						x,
						y,
						nrp->is_burning,
						nrp->is_exit_node
						);
			}
			break;
		default:
			LOG("ptype: %d\n", p->type);
			ASSERT(0);
	}

}

static inline
void read_sensors(struct sensor_readings *r) {
	SENSORS_ACTIVATE(light_sensor);
	/* 4 times are needed to better reflect the actual value right now. */
	clock_delay(400);
	r->light = light_sensor.value(LIGHT_SENSOR_PHOTOSYNTHETIC);
	/*other sensors here*/
	SENSORS_DEACTIVATE(light_sensor);
	//LOG("SENSORS: Light: %d\n", r->light);
}

static inline
metric_t sensor_readings_to_metric(const struct sensor_readings *r) {
	return r->light;
}

static
int is_emergency(metric_t metric) {
	if (metric > LIGHT_EMERGENCY_THRESHOLD) {
		return 1;
	}

	return 0;
}

//static
//int emergency_poll() {
//	struct sensor_readings r;
//	read_sensors(&r);
//
//	/* we could implement more sensor readings here. but will suffice with
//	 * light for demo. */
//	if (r.light > LIGHT_EMERGENCY_THRESHOLD) {
//		return 1;
//	}
//
//	return 0;
//}

static
int abrupt_metric_change_poll(metric_t *metric) {
	struct sensor_readings r;
	uint16_t csm;
	read_sensors(&r);

	*metric = sensor_readings_to_metric(&r);
	uint8_to_uint16(g_np.current_sensors_metric, &csm);

	if(abs(((int16_t)*metric) - ((int16_t)csm)) > ABRUPT_METRIC_CHANGE_THRESHOLD) {
		return 1;
	}

	return 0;
}

const static struct ec_callbacks ec_cb = {ec_broadcasts_recv, 
	ec_neighbors_recv, ec_timesynch_recv, ec_mesh_recv};

PROCESS(fire_process, "EmergencyWSN");
AUTOSTART_PROCESSES(&fire_process);

PROCESS_THREAD(fire_process, ev, data) {

	static struct etimer emergency_check_timer;
	PROCESS_EXITHANDLER(ec_close(&g_np.c));

	PROCESS_BEGIN();
	memset(&g_np, 0, sizeof(struct node_properties));
	neighbors_init(&g_np.ns);
	QUEUE_BUFFER_INIT_WITH_STRUCT(&g_np, emergency_coords, 
			sizeof(struct coordinate), MAX_FIRE_COORDINATES);
	ec_open(&g_np.c, EMERGENCYNET_CHANNEL, &ec_cb);
	{
//#ifndef EMERGENCY_COOJA_SIMULATION
		char buf[SETUP_PACKET_SIZE+MAX_NEIGHBORS*sizeof(rimeaddr_t)] = {0};
		struct setup_packet *sp = (struct setup_packet*)buf;
		if (node_properties_restore(buf, sizeof(buf))) {
			LOG("Found info on flash\n");
			setup_parse(sp, 1);
		}
//#endif
	}

	SENSORS_ACTIVATE(button_sensor);

	while(1) {
		etimer_set(&emergency_check_timer, CLOCK_SECOND * 1);
		PROCESS_WAIT_EVENT();

		if (ev == sensors_event) {
			if (data == &button_sensor) {
				leds_on(LEDS_ALL);
				g_np.state.is_awaiting_setup_packet = 1;
			}
		} else if (ev == serial_line_event_message && data != NULL) {
#if EMERGENCY_COOJA_SIMULATION == 1
#include "cooja_simulation1.impl"
#elif EMERGENCY_COOJA_SIMULATION == 2
#include "cooja_simulation2.impl"
#endif
			} else if(strcmp(data, "init") == 0) {
				rimeaddr_t addr = { {0xFE, 0xFE} };
				struct sensor_packet sp = {INITIALIZE_BEST_PATHS_PACKET};
				LOG("Initing\n");
				ec_broadcasts_recv(NULL, &addr, &addr, 0, 0, &sp, sizeof(struct sensor_packet));
			} else if(strcmp(data, "blink") == 0) {
				leds_blink();
		//	} else if(strcmp(data, "burn") == 0) {
		//		struct emergency_packet ep;
		//		struct sensor_readings r;
		//		LOG("SENSED EMERGENCY\n");

		//		blinking_init();

		//		read_sensors(&r);
		//		uint16_to_uint8(100, g_np.current_sensors_metric);

		//		ep.type = EMERGENCY_PACKET;
		//		coordinate_copy(&ep.source, &coordinate_node);

		//		add_coordinate_as_burning(&coordinate_node);

		//		ec_broadcast(&g_np.c, &rimeaddr_node_addr,
		//				&rimeaddr_node_addr, 0, g_np.seqno++, &ep,
		//				sizeof(struct emergency_packet));

		//		broadcast_best_path();

		//		/* Leader node should synchronize every 30 sec. New leader
		//		 * should be elected if we hit timeout of 60 sec. */
		//		ec_timesynch_network(&g_np.c);

		//		set_burning(1);
		//		g_np.state.has_sensed_emergency = 1;
		//	} else if(strcmp(data, "inc") == 0) {
		//		uint16_t now;
		//		uint8_to_uint16(g_np.current_sensors_metric, &now);
		//		uint16_to_uint8(now+100, g_np.current_sensors_metric);
		//		LOG("Current sensor metric: (%d.%d)\n",
		//				g_np.current_sensors_metric[0],
		//				g_np.current_sensors_metric[1]);
		//		broadcast_best_path();
		//	} else if(strcmp(data, "dec") == 0) {
		//		uint16_t now;
		//		uint8_to_uint16(g_np.current_sensors_metric, &now);
		//		uint16_to_uint8(now-100, g_np.current_sensors_metric);
		//		LOG("Current sensor metric: (%d.%d)\n",
		//				g_np.current_sensors_metric[0],
		//				g_np.current_sensors_metric[1]);
		//		broadcast_best_path();

			/** GUI STUFF **/
			} else if(strcmp(data, "id") == 0) {
				LOG("Id: %d.%d, Coord: (%d.%d, %d.%d), is_exit_node: %d\n", rimeaddr_node_addr.u8[0],
						rimeaddr_node_addr.u8[1], 
						coordinate_node.x[0], 
						coordinate_node.x[1], 
						coordinate_node.y[0], 
						coordinate_node.y[1],
						g_np.state.is_exit_node);
			} else if(strcmp(data, "sink") == 0) {
				uint8_t tmp[SETUP_PACKET_SIZE+1*sizeof(rimeaddr_t)] = {0};
				struct setup_packet *sp = (struct setup_packet*)tmp;
				sp->type = SETUP_PACKET;
				setup_parse(sp,0);
				g_np.state.is_sink_node = 1;
			} else if(strcmp(data, "extract_report_packet") == 0) {
				struct sensor_packet p;
				p.type = EXTRACT_REPORT_PACKET;
				ec_broadcast(&g_np.c, &rimeaddr_node_addr, &rimeaddr_node_addr,
						0, g_np.seqno++, &p, sizeof(struct sensor_packet));

			} else if(strncmp(data, "send_setup_packet", 
						sizeof("send_setup_packet")-1) == 0) {

				/* send_setup_packet:addr[0].addr[1]:coordx.coordy:is_exit_node:
				 * neighbor1[0].neighbor1[1]:
				 * neighbor2[0].neighbor2[1]:
				 * neighbor3[0].neighbor3[1] 
				 * ... 
				 * 
				 * eg:
				 * send_setup_packet:1.0:100.110:1:2.0:3.0:4.0:5.0
				 *
				 * */
				uint8_t tmp[SETUP_PACKET_SIZE+
					MAX_NEIGHBORS*sizeof(rimeaddr_t)] = {0};
				struct setup_packet *sp = (struct setup_packet*)tmp;
				const char *entry = strtok(data,":");

				sp->type = SETUP_PACKET;

				ASSERT(entry != NULL);
				entry = strtok(NULL, ".");
				ASSERT(entry != NULL);

				/* parse addr */
				sp->new_addr.u8[0] = (uint8_t)atoi(entry);
				entry = strtok(NULL, ":");
				ASSERT(entry != NULL);
				sp->new_addr.u8[1] = (uint8_t)atoi(entry);

				/* parse coord */
				entry = strtok(NULL, ".");
				ASSERT(entry != NULL);
				{
					uint16_t coord = (uint16_t)atoi(entry);
					uint16_to_uint8(coord, sp->new_coord.x);
					entry = strtok(NULL, ":");
					ASSERT(entry != NULL);
					coord = (uint16_t)atoi(entry);
					uint16_to_uint8(coord, sp->new_coord.y);
				}

				/* parse is_exit_node */
				entry = strtok(NULL, ":");
				ASSERT(entry != NULL);
				sp->is_exit_node = (int8_t)atoi(entry);

				/* parse neighbors */
				{
					uint8_t num_neighbors = 0;
					rimeaddr_t *neighbor = sp->neighbors;
					while((entry = strtok(NULL, ".")) != NULL) {
						neighbor->u8[0] = (uint8_t)atoi(entry);
						entry = strtok(NULL, ":");
						ASSERT(entry != NULL);
						neighbor->u8[1] = (uint8_t)atoi(entry);
						++neighbor;
						++num_neighbors;
					}

					ASSERT(num_neighbors != 0);
					sp->num_neighbors = num_neighbors;
				}

				{
					/* send setup_packet */
					uint8_t data_len = SETUP_PACKET_SIZE +
						sp->num_neighbors*sizeof(rimeaddr_t);
					LOG("Sending setup packet: new_addr: %d.%d, coord: (%d.%d,%d.%d), is_exit_node: %d, "
							"num_neighbors: %d, neighbors: ", 
							sp->new_addr.u8[0],
							sp->new_addr.u8[1],
							sp->new_coord.x[0],
							sp->new_coord.x[1],
							sp->new_coord.y[0],
							sp->new_coord.y[1],
							sp->is_exit_node,
							sp->num_neighbors);
					{
						uint8_t n;
						for(n = 0; n < sp->num_neighbors; ++n) {
							LOG("%d.%d, ", 
									sp->neighbors[n].u8[0],
									sp->neighbors[n].u8[1]);
						}
					}
					LOG("\n");

					ec_broadcast(&g_np.c, &rimeaddr_node_addr, &rimeaddr_node_addr,
							0, g_np.seqno++, sp, data_len);
				}
			} else {
				LOG("unkown command\n");
			}
		} else if(etimer_expired(&emergency_check_timer)) {
			if(!g_np.state.is_awaiting_setup_packet || !g_np.state.is_sink_node) {
				static metric_t current_metric;
				if (abrupt_metric_change_poll(&current_metric)) {
					uint16_to_uint8(current_metric,
							g_np.current_sensors_metric);
					if (is_emergency(current_metric)) {
						/* we sensed emergency! */
						if (g_np.bpn != NULL) {
							if (!g_np.state.is_burning) {
								LOG("Sensed emergency\n");
								set_burning(1);
								blinking_init();
								add_coordinate_as_burning(&coordinate_node);


								send_emergency_packet();
								ec_timesynch_network(&g_np.c);

								broadcast_best_path();
							}
						} else {
							LOG("Sensed emergency, but not initialized\n");
						}
					} else if (g_np.state.is_blinking /* TODO: should probably
														 change this to
														 something like
														 has_system_sensed_fire
														 */) {
						if (g_np.state.is_burning) {
							/* we're not burning anymore */
							LOG("Sensed ANTI emergency\n");
							set_burning(0);
							remove_coordinate_as_burning(&coordinate_node);
							send_anti_emergency_packet();
							blinking_update();
						}

						broadcast_best_path();
					}
				}
			}
		}
	}

	PROCESS_END();
}
