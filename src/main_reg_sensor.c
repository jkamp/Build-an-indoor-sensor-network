#include "contiki.h"

#include "dev/leds.h"
#include "dev/button-sensor.h"
#include "dev/serial-line.h"
#include "dev/light-sensor.h"

#include "sys/rtimer.h"
#include "net/rime/timesynch.h"

//#include "string.h"
//#include "limits.h"

#include "emergency_net/emergency_conn.h"
#include "emergency_net/neighbors.h"

#include "base/log.h"

/* 0 means no simulation */
#define EMERGENCY_COOJA_SIMULATION 2

#define EMERGENCYNET_CHANNEL 128
#define BLINKING_SEQUENCE_LENGTH 4
#define BLINKING_SEQUENCE_SWITCH_TIME (4*1024)
#define MAX_FIRE_COORDINATES 10

#define LIGHT_EMERGENCY_THRESHOLD 150
#define ABRUPT_METRIC_CHANGE_THRESHOLD 50


/****** PACKET TYPES ******/
enum {
	/* System in action */
	EMERGENCY_PACKET,
	BEST_PATH_UPDATE_PACKET,

	/* Setup of the system */
	SETUP_PACKET,

	INITIALIZE_BEST_PATHS_PACKET,

	/* Contains: 
	 * - Coordinate 
	 * - Best path */
	NODE_INFO_PACKET,

	RESET_SYSTEM_PACKET
};

struct sensor_packet {
	uint8_t type;
};

struct emergency_packet {
	uint8_t type;
	struct coordinate source;
};

struct best_path_update_packet {
	uint8_t type;
	struct neighbor_node_best_path bp;
};

/* GUI setup packet. */
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

enum blinking_sequence_state {
	ON = 0, OFF_1 = 1, OFF_2 = 2, OFF_3 = 3
};

struct sensor_readings {
	uint16_t light;
	/*uint16_t temp;
	uint16_t humidity;*/
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
				leds_blue(0);
				break;
			case OFF_2:
				leds_blue(0);
				break;
			case OFF_3:
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
		g_np.blinking.state = (g_np.bpn->bp.hops+1+blinking_sequences_next) %
			BLINKING_SEQUENCE_LENGTH;
	} 
}

static void
blinking_init() {
	if (!g_np.state.is_blinking) {
		g_np.state.is_blinking = 1;
		leds_blue(1);
	}
}

void setup_parse(const struct setup_packet *sp) {
	int i = 0;
	const rimeaddr_t *addr = sp->neighbors;

	rimeaddr_set_node_addr((rimeaddr_t*)&sp->new_addr);

	coordinate_set_node_coord(&sp->new_coord);

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

	LOG("Id: %d.%d, Coord: (%d, %d), ", rimeaddr_node_addr.u8[0],
			rimeaddr_node_addr.u8[1], coordinate_node.x, coordinate_node.y);
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
void best_neighbor_bp_to_our_bp(struct neighbor_node_best_path *ourbp) {
	ASSERT(g_np.bpn != NULL);
	ourbp->metric = neighbor_node_metric(g_np.bpn) +
		weigh_metrics(neighbor_node_distance(g_np.bpn));
	rimeaddr_copy(&ourbp->points_to, neighbor_node_addr(g_np.bpn));
	ourbp->hops = neighbor_node_hops(g_np.bpn) + 1;
}

static void
broadcast_best_path() {
	struct best_path_update_packet bpup;

	bpup.type = BEST_PATH_UPDATE_PACKET;
	best_neighbor_bp_to_our_bp(&bpup.bp);

	LOG("SENDING BEST_PATH_UPDATE_PACKET: metric: %d, points_to: "
			"%d.%d, hops: %d\n",
			bpup.bp.metric, 
			bpup.bp.points_to.u8[0],
			bpup.bp.points_to.u8[1], 
			bpup.bp.hops);

	ec_async_broadcast_ns(&g_np.c, &rimeaddr_node_addr, &rimeaddr_node_addr,
			0, g_np.seqno++, &bpup, sizeof(struct best_path_update_packet));
}

static void
broadcast_new_path_if_changed(const struct neighbor_node *sender) {
	const struct neighbor_node *best = find_best_exit_path_neighbor();
	ASSERT(best != NULL);

	if (g_np.bpn == NULL || 
			neighbor_node_metric(best) < neighbor_node_metric(g_np.bpn)) {
		/* Neighbor now has the new best path */
		g_np.bpn = best;
		broadcast_best_path();
		blinking_update();
	} else if (sender == best) {
		/* Neighbor updated its metrics. Broadcast changes. */
		broadcast_best_path();
		blinking_update();
	}
}

static void send_node_info() {
	struct node_info_packet nip;
	const struct neighbor_node *best =
		find_best_exit_path_neighbor();
	ASSERT(best != NULL);
	g_np.bpn = best;

	nip.type = NODE_INFO_PACKET;
	nip.coord = coordinate_node;
	best_neighbor_bp_to_our_bp(&nip.bp);

	LOG("SENDING NODE_INFO_PACKET: coord: (%d,%d), metric: %d, "
			"points_to: %d.%d, hops: %d\n",
			nip.coord.x, nip.coord.y,
			nip.bp.metric, 
			nip.bp.points_to.u8[0],
			nip.bp.points_to.u8[1],
			nip.bp.hops);

	ec_async_broadcast_ns(&g_np.c,
			&rimeaddr_node_addr, &rimeaddr_node_addr, 0,
			g_np.seqno++, &nip, sizeof(struct node_info_packet));
	g_np.state.has_sent_node_info = 1;
}

/* Received packets from everyone (whose packets are sent with
 * ec_async_broadcast) */
static void ec_recv_broadcasts(struct ec *c, const rimeaddr_t *originator, 
		const rimeaddr_t *sender, uint8_t hops, uint8_t seqno, 
		const void *data, uint8_t data_len) {
	const struct sensor_packet *p = (struct sensor_packet*)data;

	switch(p->type) {
		case EMERGENCY_PACKET:
			{
				struct emergency_packet *ep = (struct emergency_packet*)p;
				queue_buffer_push_front(&g_np.emergency_coords, &ep->source);

				/* forward packet */
				ec_async_broadcast(&g_np.c, originator, &rimeaddr_node_addr,
						hops+1, seqno, data, data_len);

				blinking_init();
			}
			break;
		case INITIALIZE_BEST_PATHS_PACKET:
			LOG("RECV INITIALIZE_BEST_PATHS_PACKET\n");
			ec_async_broadcast_ns(&g_np.c,
					originator, &rimeaddr_node_addr, hops+1,
					seqno, data, data_len);

			if(g_np.state.is_exit_node) {
				send_node_info();
			}
			break;
		case SETUP_PACKET:
			LOG("RECV SETUP_PACKET\n");
			if (g_np.state.is_awaiting_setup_packet) {
				leds_blink();
				setup_parse((const struct setup_packet*)p);
			}
			break;
		case RESET_SYSTEM_PACKET:
			{
				ec_async_broadcast_ns(&g_np.c,
						originator, &rimeaddr_node_addr, hops+1,
						seqno, data, data_len);
				g_np.state.is_blinking = 0;
				g_np.state.has_sensed_emergency = 0;
				g_np.state.is_exit_node = 0;
				g_np.state.has_sent_node_info = 0;
				leds_off(LEDS_ALL);
				ASSERT(0); /*not implemented yet*/
			}
			break;
		default:
			LOG("Unknown application packet type\n");
			ASSERT(0);
	}
}

/* Received packets from neighbors only */
static void ec_recv_neighbors(struct ec *c, const rimeaddr_t *originator, 
		const rimeaddr_t *sender, uint8_t hops, uint8_t seqno, 
		const void *data, uint8_t data_len) {
	const struct sensor_packet *p = (struct sensor_packet*)data;

	switch(p->type) {
		case BEST_PATH_UPDATE_PACKET:
			{
				const struct best_path_update_packet *bpup = 
					(struct best_path_update_packet*)p;
				struct neighbor_node *nn =
					neighbors_find_neighbor_node(&g_np.ns, sender);
				LOG("RECV BEST_PATH_UPDATE_PACKET: metric: %d, points_to: "
						"%d.%d, hops: %d\n",
						bpup->bp.metric, 
						bpup->bp.points_to.u8[0],
						bpup->bp.points_to.u8[1], 
						bpup->bp.hops);
				ASSERT(nn != NULL);

				neighbor_node_set_best_path(nn, &bpup->bp);

				/* The shortest path could have changed. */
				broadcast_new_path_if_changed(nn);
			}
			break;
		case INITIALIZE_BEST_PATHS_PACKET:
			LOG("RECV INITIALIZE_BEST_PATHS_PACKET\n");
			ec_async_broadcast_ns(&g_np.c,
					originator, &rimeaddr_node_addr, hops+1,
					seqno, data, data_len);

			if(g_np.state.is_exit_node) {
				send_node_info();
			}
			break;
		case NODE_INFO_PACKET:
			{
				const struct node_info_packet *nip = (struct node_info_packet*)p;
				struct neighbor_node *nn = neighbors_find_neighbor_node(&g_np.ns, sender);
				ASSERT(nn != NULL);
				LOG("RECV NODE_INFO_PACKET: coord: (%d,%d), metric: %d, "
						"points_to: %d.%d, hops: %d\n",
						nip->coord.x, nip->coord.y,
						nip->bp.metric,
					   	nip->bp.points_to.u8[0],
						nip->bp.points_to.u8[1],
						nip->bp.hops);
				neighbor_node_set_coordinate(nn, &nip->coord);
				neighbor_node_set_best_path(nn, &nip->bp);

				if (!g_np.state.has_sent_node_info) {
					send_node_info();
				} else {
					/* send update only if we found a better path */
					if (!g_np.state.is_exit_node) {
						broadcast_new_path_if_changed(nn);
					}
				}
			}
			break;
		default:
			LOG("Unknown application packet type\n");
			ASSERT(0);
	}
}

static void
ec_timesynch(struct ec *c) {
	/* System got timesynched */
	blinking_update();
}

static inline
void read_sensors(struct sensor_readings *r) {
	SENSORS_ACTIVATE(light_sensor);
	r->light = light_sensor.value(LIGHT_SENSOR_PHOTOSYNTHETIC);
	/*other sensors here*/
	SENSORS_DEACTIVATE(light_sensor);
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
int abrupt_metric_change_poll() {
	metric_t metric;
	struct sensor_readings r;
	read_sensors(&r);

	metric = sensor_readings_to_metric(&r);

	if(abs(metric - g_np.current_sensors_metric) > ABRUPT_METRIC_CHANGE_THRESHOLD) {
		g_np.current_sensors_metric = metric;
		return 1;
	}

	return 0;
}

const static struct ec_callbacks ec_cb = {ec_recv_broadcasts, 
	ec_recv_neighbors, ec_timesynch};

PROCESS(fire_process, "EmergencyWSN");
AUTOSTART_PROCESSES(&fire_process);

PROCESS_THREAD(fire_process, ev, data) {

	static struct etimer emergency_check_timer;
	PROCESS_EXITHANDLER(ec_close(&g_np.c));

	PROCESS_BEGIN();
	memset(&g_np, 0, sizeof(struct node_properties));
	neighbors_init(&g_np.ns);
	ec_open(&g_np.c, EMERGENCYNET_CHANNEL, &ec_cb);
	ec_timesynch_on(&g_np.c);

	SENSORS_ACTIVATE(button_sensor);

	while(1) {
		etimer_set(&emergency_check_timer, CLOCK_SECOND * 1);
		PROCESS_WAIT_EVENT();

		if (ev == sensors_event) {
#ifdef EMERGENCY_COOJA_SIMULATION
			if (data == &button_sensor) {
				/* simulate emergency */
				struct emergency_packet ep;
				LOG("I SENSED EMERGENCY\n");
				ep.type = EMERGENCY_PACKET;
				ep.source = coordinate_node;
				queue_buffer_push_front(&g_np.emergency_coords, &coordinate_node);

				//g_np.state.has_sensed_emergency = 1;
				//g_np.current_metric += 100;

				ec_async_broadcast_ns(&g_np.c, &rimeaddr_node_addr,
						&rimeaddr_node_addr, 0, g_np.seqno++, &ep,
						sizeof(struct emergency_packet));

				broadcast_best_path();

				/* Leader node should synchronize every 30 sec. New leader
				 * should be elected if we hit timeout of 60 sec. */
				ec_timesynch_network(&g_np.c);

				leds_red(1);
				blinking_init();
			}
#else
			if (data == &button_sensor) {
				leds_on();
				g_np.state.is_awaiting_setup_packet = 1;
			}
#endif
		} else if (ev == serial_line_event_message && data != NULL) {
#if EMERGENCY_COOJA_SIMULATION == 1
#include "cooja_simulation1.impl"
#elif EMERGENCY_COOJA_SIMULATION == 2
#include "cooja_simulation2.impl"
#else
			if(strcmp(data, "init") == 0) {
				rimeaddr_t addr = { {0xFF, 0xFF} };
				struct sensor_packet sp = {INITIALIZE_BEST_PATHS_PACKET};
				LOG("Initing\n");
				ec_recv(NULL, &addr, &addr, 0, 0, &sp, sizeof(struct sensor_packet));
			}
#endif
		} else if(etimer_expired(&emergency_check_timer)) {
			if(!g_np.state.has_sensed_emergency) {
				if (emergency_poll()) {
					/* we sensed emergency! */
					struct emergency_packet ep;
					LOG("SENSED EMERGENCY\n");
					ep.type = EMERGENCY_PACKET;
					ep.source = coordinate_node;

					queue_buffer_push_front(&g_np.emergency_coords, &coordinate_node);

					ec_async_broadcast(&g_np.c, &rimeaddr_node_addr,
							&rimeaddr_node_addr, 0, g_np.seqno++, &ep,
							sizeof(struct emergency_packet));

					broadcast_best_path();

					/* Leader node should synchronize every 30 sec. New leader
					 * should be elected if we hit timeout of 60 sec. */
					ec_timesynch_network(&g_np.c);

					g_np.state.has_sensed_emergency = 1;
				}
			} else {
				if (abrupt_metric_change_poll()) {
					broadcast_best_path();
					if(emergency_poll()) {
						leds_red(1);
					} else {
						leds_red(0);
					}
				}
			}
		}
	}

	PROCESS_END();
}
