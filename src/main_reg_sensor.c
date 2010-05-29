#include "contiki.h"

#include "dev/leds.h"
#include "dev/button-sensor.h"
#include "dev/serial-line.h"
#include "dev/light-sensor.h"

#include "dev/sky-sensors.h"

#include "sys/rtimer.h"
#include "net/rime/timesynch.h"

//#include "string.h"
//#include "limits.h"

#include "emergency_net/emergency_conn.h"
#include "emergency_net/neighbors.h"

#include "base/node_properties.h"

#include "base/log.h"

/* 0 means no simulation */
#define EMERGENCY_COOJA_SIMULATION 2

#define EMERGENCYNET_CHANNEL 128
#define BLINKING_SEQUENCE_LENGTH 4
#define BLINKING_SEQUENCE_SWITCH_TIME (4*1024)
#define MAX_FIRE_COORDINATES 10

#define LIGHT_EMERGENCY_THRESHOLD 300
#define ABRUPT_METRIC_CHANGE_THRESHOLD 100


/****** PACKET TYPES ******/
enum {
	/* System in action */
	EMERGENCY_PACKET=0,
	BEST_PATH_UPDATE_PACKET=1,

	/* Setup of the system */
	SETUP_PACKET=2,

	INITIALIZE_BEST_PATHS_PACKET=3,

	/* Contains: 
	 * - Coordinate 
	 * - Best path */
	NODE_INFO_PACKET=4,

	EXTRACT_INFO_PACKET=5,

	RESET_SYSTEM_PACKET=6
};

static inline
void uint16_to_uint8(const uint16_t in, uint8_t out[2]) {
	out[0] = in&0xFF00;
	out[1] = in&0xFF;
}

static inline
void uint8_to_uint16(const uint8_t in[2],
	   	uint16_t *out) {
	*out = (in[0]<<8) | in[1];
}

/* used to convert 16bit metrics to 8bits for sending */
struct best_path_aligned {
	uint8_t metric[2];
	rimeaddr_t points_to;
	uint8_t hops;
};

struct coordinate_aligned {
	uint8_t x[2];
	uint8_t y[2];
};

void coord_to_coord_aligned(const struct coordinate *c, 
		struct coordinate_aligned *ca) {
	uint16_to_uint8(c->x, ca->x);
	uint16_to_uint8(c->y, ca->y);
}

void coord_aligned_to_coord(const struct coordinate_aligned *ca, 
		struct coordinate *c) {
	uint8_to_uint16(ca->x, &c->x);
	uint8_to_uint16(ca->y, &c->y);
}

void neighbor_node_best_path_to_best_path_aligned(
		const struct neighbor_node_best_path *bp, 
		struct best_path_aligned *bpa) {
	uint16_to_uint8(bp->metric, bpa->metric);
	rimeaddr_copy(&bpa->points_to, &bp->points_to);
	bpa->hops = bp->hops;
}

void best_path_aligned_to_neighbor_node_best_path(
		const struct best_path_aligned *bpa,
		struct neighbor_node_best_path *bp) {
	uint8_to_uint16(bpa->metric, &bp->metric);
	rimeaddr_copy(&bp->points_to, &bpa->points_to);
	bp->hops = bpa->hops;
}

struct sensor_packet {
	uint8_t type;
};

/* Sent when we have sensed emergency (e.g. fire). This is broadcasted without
 * neighbor req, and flooded throughout the network. */
struct emergency_packet {
	uint8_t type;
	struct coordinate_aligned source; /* of emergency */
};

/* Sent when we have calculated a new best path. */
struct best_path_update_packet {
	uint8_t type;
	struct best_path_aligned bpa;
};

#define SETUP_PACKET_SIZE (sizeof(struct setup_packet)-sizeof(rimeaddr_t))
/* GUI setup packet. Can be sent to the node when the usr button has been
 * pushed. Is used for uploading rimeaddr, coord, if it is an exit node, and
 * the neighbors of the node. */
struct setup_packet {
	uint8_t type;
	rimeaddr_t new_addr;
	struct coordinate_aligned new_coord;
	int8_t is_exit_node;
	uint8_t num_neighbors;
	rimeaddr_t neighbors[1];
};

/* Sent when INITIALIZE_BEST_PATHS_PACKET is sent (for exit nodes) and sent
 * when regular nodes get NODE_INFO_PACKET from other nodes (not only exit
 * nodes). This to be able to calculate nearest paths (we get coordinates of
 * neighbors from this packet) and store them so we can start blinking
 * immediately and do not need to figure out best paths etc at that time. This
 * packet is only sent once to the node's neighbors. */
struct node_info_packet {
	uint8_t type;
	struct coordinate_aligned coord;
	struct best_path_aligned bpa;
};


enum blinking_sequence_state {
	ON = 0, OFF_1 = 1, OFF_2 = 2, OFF_3 = 3
	/*, OFF_4 = 4, OFF_5 = 5, OFF_6 = 6,
	OFF_7 = 7*/
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
		int8_t has_sensed_emergency;
		int8_t is_exit_node;
		int8_t is_awaiting_setup_packet;
		int8_t has_sent_node_info;
		int8_t is_reset_mode;
	} state;

	metric_t current_sensors_metric;

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
blink(struct rtimer *rt, void *ptr) {
	if (g_np.state.is_blinking) {
		switch(g_np.blinking.state) {
			case ON:
				leds_blue(1);
				break;
			case OFF_1:
			case OFF_2:
			case OFF_3:
			/*case OFF_4:
			case OFF_5:
			case OFF_6:
			case OFF_7:*/
				leds_blue(0);
				break;
		}
		g_np.blinking.next_wakeup += BLINKING_SEQUENCE_SWITCH_TIME;
		rtimer_set(&g_np.blinking.rt, g_np.blinking.next_wakeup, 0, blink, NULL);

		g_np.blinking.state = (g_np.blinking.state+1) % BLINKING_SEQUENCE_LENGTH;
	}
}

static void
blinking_update() {
	if (g_np.state.is_blinking) {
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
		/*leds_blue(1);*/
	}
}

void setup_parse(const struct setup_packet *sp, int is_from_flash) {
	int i = 0;
	const rimeaddr_t *addr = sp->neighbors;
	struct coordinate coord;

	rimeaddr_set_node_addr((rimeaddr_t*)&sp->new_addr);

	coord_aligned_to_coord(&sp->new_coord, &coord);
	coordinate_set_node_coord(&coord);

	neighbors_clear(&g_np.ns);

	for(; i < sp->num_neighbors; ++i) {
		neighbors_add(&g_np.ns, addr++);
	}
	
	if (sp->is_exit_node) {
		struct neighbor_node_best_path bp;
		bp.metric = 0;
		rimeaddr_copy(&bp.points_to, &rimeaddr_node_addr);
		bp.hops = -1; /* since we calculate +1 when sending path */

		g_np.state.is_exit_node = 1;
		neighbor_node_set_addr(&exit_node, &rimeaddr_node_addr);
		neighbor_node_set_coordinate(&exit_node, &coordinate_node);
		neighbor_node_set_best_path(&exit_node, &bp);

		leds_green(1);
	}

	LOG("Id: %d.%d, Coord: (%d, %d), is_exit_node: %d ", rimeaddr_node_addr.u8[0],
			rimeaddr_node_addr.u8[1], coordinate_node.x, coordinate_node.y, g_np.state.is_exit_node);
	LOG("Neighbors: ");
	{
		const struct neighbor_node *nn = neighbors_begin(&g_np.ns);
		for (; nn != NULL;
				nn = neighbors_next(&g_np.ns)) {
			LOG("%d.%d, ", neighbor_node_addr(nn)->u8[0],
					neighbor_node_addr(nn)->u8[1]);
		}
	}
	LOG("\n");

#ifndef EMERGENCY_COOJA_SIMULATION
	if(!is_from_flash) {
		LOG("Burning info to flash\n");
		node_properties_burn(sp, SETUP_PACKET_SIZE + 
				sp->num_neighbors * sizeof(rimeaddr_t));
		LOG("Burning info to flash OK\n");
	}
#endif
}

static inline metric_t
weigh_metrics(distance_t distance) {
	return 10*distance + g_np.current_sensors_metric;
}

static const struct neighbor_node*
find_best_exit_path_neighbor() {
	const struct neighbor_node *best = NULL;
	const struct neighbor_node *i = neighbors_begin(&g_np.ns);
	metric_t min_metric = METRIC_T_MAX;
	metric_t metric;

	for(;i != NULL; i = neighbors_next(&g_np.ns)) {
		TRACE("neighbor: %d.%d, points_to_us: %d, coord: (%d,%d), distance: %d, "
				"metric: %d\n",
				neighbor_node_addr(i)->u8[0], neighbor_node_addr(i)->u8[1],
				neighbor_node_points_to_us(i),
				neighbor_node_coord(i)->x, neighbor_node_coord(i)->y,
				neighbor_node_distance(i),
				neighbor_node_metric(i));

		if(!neighbor_node_points_to_us(i)) { 
			metric = weigh_metrics( neighbor_node_distance(i) ) +
				neighbor_node_metric(i);
			if (metric < min_metric) {
				min_metric = metric;
				best = i;
			}
		}
	}

	if (g_np.state.is_exit_node) {
		/* extra checks for exit nodes */
		if (g_np.current_sensors_metric < min_metric) {
			best = &exit_node;
		}
	}

	ASSERT(best != NULL);
	TRACE("BEST neighbor: %d.%d, points_to_us: %d, coord: (%d,%d), distance: %d, "
			"metric: %d\n",
			neighbor_node_addr(best)->u8[0], neighbor_node_addr(best)->u8[1],
			neighbor_node_points_to_us(best),
			neighbor_node_coord(best)->x, neighbor_node_coord(best)->y,
			neighbor_node_distance(best),
			neighbor_node_metric(best));

	return best;
}

/* Transforms our best neighbor's best path to our own. */
void best_neighbor_bp_to_our_bp_aligned(struct best_path_aligned *bpa) {
	ASSERT(g_np.bpn != NULL);
	uint16_to_uint8(
			neighbor_node_metric(g_np.bpn) +
			weigh_metrics(neighbor_node_distance(g_np.bpn)),
			bpa->metric
			);
	rimeaddr_copy(&bpa->points_to, neighbor_node_addr(g_np.bpn));
	bpa->hops = neighbor_node_hops(g_np.bpn) + 1;
}

static void
broadcast_best_path() {
	struct best_path_update_packet bpup;

	bpup.type = BEST_PATH_UPDATE_PACKET;
	best_neighbor_bp_to_our_bp_aligned(&bpup.bpa);
	//neighbor_node_best_path_to_best_path_aligned(&bp, &bpup.bpa);

	LOG("SENDING BEST_PATH_UPDATE_PACKET: metric: [%d %d], points_to: "
			"%d.%d, hops: %d\n",
			bpup.bpa.metric[0], 
			bpup.bpa.metric[1], 
			bpup.bpa.points_to.u8[0],
			bpup.bpa.points_to.u8[1], 
			bpup.bpa.hops);

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
	coord_to_coord_aligned(&coordinate_node, &nip.coord);
	best_neighbor_bp_to_our_bp_aligned(&nip.bpa);

	LOG("SENDING NODE_INFO_PACKET: coord: (%d.%d,%d.%d), metric: %d,%d, "
			"points_to: %d.%d, hops: %d\n",
			nip.coord.x[0], 
			nip.coord.x[1], 
			nip.coord.y[0], 
			nip.coord.y[1], 
			nip.bpa.metric[0], 
			nip.bpa.metric[1], 
			nip.bpa.points_to.u8[0],
			nip.bpa.points_to.u8[1],
			nip.bpa.hops);
	LOG("Sizeof node_info_packet: %d\n", sizeof(struct node_info_packet));

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
	struct neighbor_node *nn = (struct neighbor_node*)neighbors_begin(&g_np.ns);
	leds_on(LEDS_ALL);

	for(; nn != NULL; nn = (struct neighbor_node*)neighbors_next(&g_np.ns)) {
		neighbor_node_set_best_path(nn, &neighbor_node_best_path_max);
		neighbor_node_set_coordinate(nn, &coordinate_null);
	}

	g_np.bpn = NULL;
	g_np.state.is_blinking = 0;
	g_np.state.has_sensed_emergency = 0;
	/*g_np.state.is_exit_node = 0;*/
	g_np.state.has_sent_node_info = 0;

	queue_buffer_clear(&g_np.emergency_coords);
	g_np.current_sensors_metric = 0;

	ec_timesynch_off(&g_np.c);

	g_np.state.is_reset_mode = 1;
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

	switch(p->type) {
		case EMERGENCY_PACKET:
			{
				struct emergency_packet *ep = (struct emergency_packet*)p;
				struct coordinate coord;
				coord_aligned_to_coord(&ep->source, &coord);
				LOG("RECV EMERGENCY_PACKET: coord: %d, %d\n", coord.x, coord.y);
				queue_buffer_push_front(&g_np.emergency_coords, &coord);

				/* forward packet */
				ec_broadcast(&g_np.c, originator, &rimeaddr_node_addr,
						hops+1, seqno, data, data_len);

				blinking_init();
			}
			break;
		case INITIALIZE_BEST_PATHS_PACKET:
			LOG("RECV INITIALIZE_BEST_PATHS_PACKET\n");
			ec_reliable_broadcast_ns(&g_np.c,
					originator, &rimeaddr_node_addr, hops+1,
					seqno, data, data_len);

			initialize_best_path_packet_handler();
			break;
		case SETUP_PACKET:
			LOG("RECV SETUP_PACKET\n");
			if (g_np.state.is_awaiting_setup_packet) {
				g_np.state.is_awaiting_setup_packet = 0;
				setup_parse((const struct setup_packet*)p, 0);
				leds_blink();
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

	switch(p->type) {
		case BEST_PATH_UPDATE_PACKET:
			{
				const struct best_path_update_packet *bpup = 
					(struct best_path_update_packet*)p;
				struct neighbor_node *nn =
					neighbors_find_neighbor_node(&g_np.ns, sender);
				struct neighbor_node_best_path bp;
				best_path_aligned_to_neighbor_node_best_path(&bpup->bpa, &bp);
				LOG("RECV BEST_PATH_UPDATE_PACKET: metric: %d,%d, points_to: "
						"%d.%d, hops: %d\n",
						bpup->bpa.metric[0], 
						bpup->bpa.metric[1], 
						bpup->bpa.points_to.u8[0],
						bpup->bpa.points_to.u8[1], 
						bpup->bpa.hops);
				ASSERT(nn != NULL);

				neighbor_node_set_best_path(nn, &bp);

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
				struct coordinate coord;
				struct neighbor_node_best_path bp;
				coord_aligned_to_coord(&nip->coord, &coord);
				best_path_aligned_to_neighbor_node_best_path(&nip->bpa, &bp);
				LOG("RECV NODE_INFO_PACKET: coord: (%d.%d,%d.%d), metric: %d,%d, "
						"points_to: %d.%d, hops: %d\n",
						nip->coord.x[0], 
						nip->coord.x[1], 
						nip->coord.y[0], 
						nip->coord.y[1], 
						nip->bpa.metric[0], 
						nip->bpa.metric[1], 
						nip->bpa.points_to.u8[0],
						nip->bpa.points_to.u8[1],
						nip->bpa.hops);
				LOG("Sizeof node_info_packet2: %d\n", sizeof(struct node_info_packet));
				print_packet_data((uint8_t*)nip, sizeof(struct node_info_packet));
				ASSERT(nn != NULL);
				neighbor_node_set_coordinate(nn, &coord);
				neighbor_node_set_best_path(nn, &bp);

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
	case EXTRACT_INFO_PACKET:
		LOG("RECV EXTRACT_INFO_PACKET\n");
		break;
	default:
		ASSERT(0);
	}

}

static inline
void read_sensors(struct sensor_readings *r) {
	SENSORS_ACTIVATE(light_sensor);
	/* Two times are needed to better reflect the actual value right now. */
	r->light = light_sensor.value(LIGHT_SENSOR_PHOTOSYNTHETIC);
	r->light = light_sensor.value(LIGHT_SENSOR_PHOTOSYNTHETIC);
	/*other sensors here*/
	SENSORS_DEACTIVATE(light_sensor);
	LOG("SENSORS: Light: %d\n", r->light);
}

static inline
metric_t sensor_readings_to_metric(const struct sensor_readings *r) {
	return r->light;
}


static
int emergency_poll() {
	struct sensor_readings r;
	read_sensors(&r);

	/* we could implement more sensor readings here. but will suffice with
	 * light for demo. */
	if (r.light > LIGHT_EMERGENCY_THRESHOLD) {
		return 1;
	}

	return 0;
}

static
int abrupt_metric_change_poll(metric_t *metric) {
	struct sensor_readings r;
	read_sensors(&r);

	*metric = sensor_readings_to_metric(&r);

	if(abs(*metric - g_np.current_sensors_metric) > 
			ABRUPT_METRIC_CHANGE_THRESHOLD) {
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
#ifndef EMERGENCY_COOJA_SIMULATION
		char buf[SETUP_PACKET_SIZE+MAX_NEIGHBORS*sizeof(rimeaddr_t)] = {0};
		struct setup_packet *sp = (struct setup_packet*)buf;
		if (node_properties_restore(buf, sizeof(buf))) {
			LOG("Found info on flash\n");
			setup_parse(sp, 1);
		}
#endif
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
#ifdef EMERGENCY_COOJA_SIMULATION
			} else if(strcmp(data, "init") == 0) {
				rimeaddr_t addr = { {0xFE, 0xFE} };
				struct sensor_packet sp = {INITIALIZE_BEST_PATHS_PACKET};
				LOG("Initing\n");
				ec_broadcasts_recv(NULL, &addr, &addr, 0, 0, &sp, sizeof(struct sensor_packet));
			} else if(strcmp(data, "blink") == 0) {
				leds_blink();
				leds_blink();
				leds_blink();
				leds_blink();
			} else if(strcmp(data, "poll") == 0) {
				if (emergency_poll()) {
					LOG("Polled emergency returned 1\n");
				}
			} else if(strcmp(data, "burn") == 0) {
				struct emergency_packet ep;
				struct sensor_readings r;
				LOG("SENSED EMERGENCY\n");

				blinking_init();

				read_sensors(&r);
				g_np.current_sensors_metric = 100;/*sensor_readings_to_metric(&r);*/

				ep.type = EMERGENCY_PACKET;
				coord_to_coord_aligned(&coordinate_node, &ep.source);

				queue_buffer_push_front(&g_np.emergency_coords, &coordinate_node);

				ec_broadcast(&g_np.c, &rimeaddr_node_addr,
						&rimeaddr_node_addr, 0, g_np.seqno++, &ep,
						sizeof(struct emergency_packet));

				broadcast_best_path();

				/* Leader node should synchronize every 30 sec. New leader
				 * should be elected if we hit timeout of 60 sec. */
				ec_timesynch_network(&g_np.c);

				leds_red(1);
				g_np.state.has_sensed_emergency = 1;
			} else if(strcmp(data, "inc") == 0) {
				g_np.current_sensors_metric += 10;
				LOG("Current sensor metric: %d\n", g_np.current_sensors_metric);
				broadcast_best_path();
			} else if(strcmp(data, "dec") == 0) {
				g_np.current_sensors_metric -= 10;
				LOG("Current sensor metric: %d\n", g_np.current_sensors_metric);
				broadcast_best_path();
			} else {
				LOG("unkown command\n");
			}
#else
		} else if(etimer_expired(&emergency_check_timer)) {
			if(!g_np.state.has_sensed_emergency) {
				if (emergency_poll()) {
					/* we sensed emergency! */
					struct emergency_packet ep;
					struct sensor_readings r;
					LOG("SENSED EMERGENCY\n");

					blinking_init();

					read_sensors(&r);
					g_np.current_sensors_metric = sensor_readings_to_metric(&r);

					ep.type = EMERGENCY_PACKET;
					ep.source = coordinate_node;

					queue_buffer_push_front(&g_np.emergency_coords, &coordinate_node);

					ec_broadcast(&g_np.c, &rimeaddr_node_addr,
							&rimeaddr_node_addr, 0, g_np.seqno++, &ep,
							sizeof(struct emergency_packet));

					broadcast_best_path();

					/* Leader node should synchronize every 30 sec. New leader
					 * should be elected if we hit timeout of 60 sec. */
					ec_timesynch_network(&g_np.c);

					g_np.state.has_sensed_emergency = 1;
				}
			} else {
				static metric_t current_metric;
				if (abrupt_metric_change_poll(&current_metric)) {
					g_np.current_sensors_metric = current_metric;
					broadcast_best_path();
					if(emergency_poll()) {
						leds_red(1);
					} else {
						leds_red(0);
					}
				}
			}
#endif
		}
	}

	PROCESS_END();
}
