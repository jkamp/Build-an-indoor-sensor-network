#include "contiki.h"

#include "dev/leds.h"
#include "dev/button-sensor.h"
#include "dev/serial-line.h"
//#include "dev/light-sensor.h"

#include "sys/rtimer.h"
#include "net/rime/timesynch.h"

#include "string.h"
#include "limits.h"

#include "emergency_net/emergency_conn.h"
#include "emergency_net/neighbors.h"

#include "base/log.h"

/* 0 means no simulation */
#define EMERGENCY_COOJA_SIMULATION 1

#define EMERGENCYNET_CHANNEL 128
#define BLINKING_SEQUENCE_LENGTH 4
#define BLINKING_SEQUENCE_SWITCH_TIME 1024
#define MAX_FIRE_COORDINATES 10


/****** PACKET TYPES ******/
enum {
	/* System in action */
	EMERGENCY_PACKET,
	BEST_PATH_UPDATE_PACKET,

	/* Setup of the system */
	START_BLINKING_PACKET,
	STOP_BLINKING_PACKET,
	SETUP_PACKET,

	NEW_NEIGHBOR_PACKET,
	INITIALIZE_BEST_PATHS_PACKET,

	/* Contains: 
	 * - Coordinate 
	 * - Hops to nearest exit
	 * - Shortest distance to nearest exit
	 * - Through what node is the shortest distance calculated */
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
	struct best_path bp;
};

/* GUI setup packet. */
#define SETUP_PACKET_SIZE (sizeof(struct setup_packet)-sizeof(uint8_t))
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
	struct best_path bp;
};

enum blinking_sequence_state {
	ON = 0, OFF_1 = 1, OFF_2 = 2, OFF_3 = 3
};

struct node_properties {
	struct neighbors ns;
	const struct neighbor_node *bpn; /* best path neighbor */

	QUEUE_BUFFER(emergency_coords, sizeof(struct coordinate), MAX_FIRE_COORDINATES);

	struct {
		struct rtimer rt;
		rtimer_clock_t next_wakeup;
		enum blinking_sequence_state state;
	} blinking;

	struct {
		int8_t is_blinking;
		int8_t is_aware_of_emergency;
		int8_t has_sensed_emergency;
		int8_t is_exit_node;
		int8_t has_sent_node_info;
	} state;

	struct ec c;
	uint8_t seqno;
};

static struct node_properties g_np;

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
	TRACE("Updating blinking\n");
	if (g_np.state.is_blinking) {
		uint16_t blinking_sequences_next = (timesynch_time() /
				BLINKING_SEQUENCE_SWITCH_TIME)+1;
		g_np.blinking.next_wakeup =
			timesynch_time_to_rtimer(blinking_sequences_next*
					BLINKING_SEQUENCE_SWITCH_TIME);

		rtimer_set(&g_np.blinking.rt, g_np.blinking.next_wakeup, 0, blink,
				NULL);

		if(!g_np.state.is_exit_node) {
			ASSERT(g_np.bpn != NULL);
			g_np.blinking.state = (g_np.bpn->bp.hops+1+blinking_sequences_next) %
				BLINKING_SEQUENCE_LENGTH;
		} else {
			g_np.blinking.state = blinking_sequences_next %
				BLINKING_SEQUENCE_LENGTH;
		}
	} 
}

static void
blinking_on() {
	if (!g_np.state.is_blinking) {
		g_np.state.is_blinking = 1;
		blinking_update();
	}
}

void setup_parse(const struct setup_packet *sp) {
	int i = 0;
	const rimeaddr_t *addr = sp->neighbors;

	//FIXME(johan): Not so good since ACK will be from different ID!
	rimeaddr_set_node_addr((rimeaddr_t*)&sp->new_addr);

	coordinate_set_node_coord(&sp->new_coord);

	neighbors_clear(&g_np.ns);

	for(; i < sp->num_neighbors; ++i) {
		neighbors_add(&g_np.ns, addr++);
	}
	
	if (sp->is_exit_node) {
		g_np.state.is_exit_node = 1;
		leds_green(1);
	}
}

static metric_t
calculate_metric(distance_t distance) {
	return distance;
}

static const struct neighbor_node*
find_best_exit_path_neighbor() {
	const struct neighbor_node *best = NULL;
	const struct neighbor_node *i = neighbors_begin(&g_np.ns);
	metric_t min_metric = METRIC_T_MAX;
	metric_t metric;

	for(;i != NULL; i = neighbors_next(&g_np.ns)) {
		if(!neighbor_node_points_to_us(i)) { 
			metric = calculate_metric( neighbor_node_distance(i) ) +
				neighbor_node_metric(i);
			if (metric < min_metric) {
				min_metric = metric;
				best = i;
			}
		}
	}

	return best;
}

/* Transforms our best neighbor's best path to our own. */
void best_neighbor_bp_to_our_bp(struct best_path *ourbp) {
	if(!g_np.state.is_exit_node) {
		ASSERT(g_np.bpn != NULL);
		ourbp->metric = neighbor_node_metric(g_np.bpn) + calculate_metric(neighbor_node_distance(g_np.bpn));
		rimeaddr_copy(&ourbp->points_to, neighbor_node_addr(g_np.bpn));
		ourbp->hops = neighbor_node_hops(g_np.bpn) + 1;
	} else {
		ourbp->metric = calculate_metric(0);
		rimeaddr_copy(&ourbp->points_to, &rimeaddr_node_addr);
		ourbp->hops = 0;
	}
}

static void
broadcast_best_path() {
	struct best_path_update_packet bpup;

	bpup.type = BEST_PATH_UPDATE_PACKET;
	best_neighbor_bp_to_our_bp(&bpup.bp);

	ec_async_broadcast_ns(&g_np.c, &rimeaddr_node_addr, &rimeaddr_node_addr,
			0, g_np.seqno++, &bpup, sizeof(struct best_path_update_packet));
}

static void
broadcast_new_path_if_changed() {
	const struct neighbor_node *best = find_best_exit_path_neighbor();
	ASSERT(best != NULL);

	if (g_np.bpn == NULL || 
			neighbor_node_metric(best) < neighbor_node_metric(g_np.bpn)) {
		/* another neighbor now has the best path */
		g_np.bpn = best;
		broadcast_best_path();
		blinking_update();
	} else if (g_np.bpn == best) {
		/* check if updated. */
		/* TODO(johan):implement hysterisis */
		if (neighbor_node_metric(g_np.bpn) != neighbor_node_metric(best)) {
			/* neighbor updated its metrics. Broadcast changes. */
			broadcast_best_path();
		}
	}
}

static void ec_recv(struct ec *c, const rimeaddr_t *originator, 
		const rimeaddr_t *sender, uint8_t hops, uint8_t seqno, 
		const void *data, uint8_t data_len) {
	const struct sensor_packet *p = (struct sensor_packet*)data;

	switch(p->type) {
		case EMERGENCY_PACKET:
			{
				struct emergency_packet *ep = (struct emergency_packet*)p;
				queue_buffer_push_front(&g_np.emergency_coords, &ep->source);

				/* forward packet */
				ec_async_broadcast_ns(&g_np.c, originator, &rimeaddr_node_addr,
						hops+1, seqno, data, data_len);

				blinking_on();
			}
			break;
		case BEST_PATH_UPDATE_PACKET:
			{
				const struct best_path_update_packet *bpup = 
					(struct best_path_update_packet*)p;
				struct neighbor_node *nn =
					neighbors_find_neighbor_node(&g_np.ns, sender);
				ASSERT(nn != NULL);

				neighbor_node_set_best_path(nn, &bpup->bp);

				/* The shortest path could have changed. */
				if (!g_np.state.is_exit_node) {
					broadcast_new_path_if_changed();
				}
			}
			break;
		case START_BLINKING_PACKET:
			ASSERT(0);
			break;
		case STOP_BLINKING_PACKET:
			ASSERT(0);
			break;
		case SETUP_PACKET:
			setup_parse((const struct setup_packet*)p);
			break;
		case NEW_NEIGHBOR_PACKET:
			ASSERT(0);
			break;

		case INITIALIZE_BEST_PATHS_PACKET:
			ec_async_broadcast_ns(&g_np.c,
					originator, &rimeaddr_node_addr, hops+1,
					seqno, data, data_len);

			if(g_np.state.is_exit_node) {
				struct node_info_packet nip;

				nip.type = NODE_INFO_PACKET;
				nip.coord = coordinate_node;
				best_neighbor_bp_to_our_bp(&nip.bp);

				ec_async_broadcast_ns(&g_np.c,
						&rimeaddr_node_addr, &rimeaddr_node_addr, 0,
						g_np.seqno++, &nip, sizeof(struct node_info_packet));
				g_np.state.has_sent_node_info = 1;
			}
			break;
		case NODE_INFO_PACKET:
			{
				const struct node_info_packet *nip = (struct node_info_packet*)p;
				struct neighbor_node *nn = neighbors_find_neighbor_node(&g_np.ns, sender);
				ASSERT(nn != NULL);
				neighbor_node_set_coordinate(nn, &nip->coord);
				neighbor_node_set_best_path(nn, &nip->bp);

				if (!g_np.state.has_sent_node_info) {
					struct node_info_packet new_nip;
					const struct neighbor_node *best = find_best_exit_path_neighbor();
					ASSERT(best != NULL);
					if (best != g_np.bpn) {
						/* update our own best path */
						g_np.bpn = best;
					}
					new_nip.type = NODE_INFO_PACKET;
					new_nip.coord = coordinate_node;
					best_neighbor_bp_to_our_bp(&new_nip.bp);

					ec_async_broadcast_ns(&g_np.c,
							&rimeaddr_node_addr, &rimeaddr_node_addr, 0,
							g_np.seqno++, &new_nip, sizeof(struct node_info_packet));
					g_np.state.has_sent_node_info = 1;
				} else {
					/* send update only if we found a better path */
					if (!g_np.state.is_exit_node) {
						broadcast_new_path_if_changed();
					}
				}
			}
			break;
		case RESET_SYSTEM_PACKET:
			ASSERT(0);
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

const static struct ec_callbacks ec_cb = {ec_recv, ec_timesynch};


PROCESS(fire_process, "FireWSN");
AUTOSTART_PROCESSES(&fire_process);

PROCESS_THREAD(fire_process, ev, data) {

	//static struct etimer fire_check_timer;
	PROCESS_EXITHANDLER(ec_close(&g_np.c));

	PROCESS_BEGIN();
	memset(&g_np, 0, sizeof(struct node_properties));
	neighbors_init(&g_np.ns);
	ec_open(&g_np.c, EMERGENCYNET_CHANNEL, &ec_cb);
	ec_timesynch_on(&g_np.c);

	SENSORS_ACTIVATE(button_sensor);

	while(1) {
		//etimer_set(&fire_check_timer, CLOCK_SECOND * 1);
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

				ec_async_broadcast_ns(&g_np.c, &rimeaddr_node_addr,
						&rimeaddr_node_addr, 0, g_np.seqno++, &ep,
						sizeof(struct emergency_packet));

				broadcast_best_path();

				/* Leader node should synchronize every 30 sec. New leader
				 * should be elected if we hit timeout of 60 sec. */
				ec_timesynch_network(&g_np.c);

				leds_red(1);
				blinking_on();
				g_np.state.has_sensed_emergency = 1;
				g_np.state.is_aware_of_emergency = 1;
			} 
#endif
		} else if (ev == serial_line_event_message && data != NULL) {
#if EMERGENCY_COOJA_SIMULATION == 1
			if (strcmp(data, "1") == 0) {
				uint8_t tmp[SETUP_PACKET_SIZE+1*sizeof(rimeaddr_t)] = {0};
				struct setup_packet *sp = (struct setup_packet*)tmp;
				rimeaddr_t *neighbor_addr = sp->neighbors;

				sp->type = SETUP_PACKET;

				{
					rimeaddr_t addr;
					addr.u8[0] = 1;
					addr.u8[1] = 0;
					rimeaddr_copy(&sp->new_addr, &addr);
				}
				{
					struct coordinate coord = {0,0};
					sp->new_coord = coord;
				}

				sp->is_exit_node = 1;
				sp->num_neighbors = 1;

				{
					rimeaddr_t addr;
					addr.u8[0] = 2;
					addr.u8[1] = 0;
					rimeaddr_copy(neighbor_addr++, &addr);
				}

				setup_parse(sp);

				ec_set_neighbors(&g_np.c, &g_np.ns);

				LOG("Im now node 1.0 (exit node)\n");
			} else if(strcmp(data, "2") == 0) {
				uint8_t tmp[SETUP_PACKET_SIZE+2*sizeof(rimeaddr_t)] = {0};
				struct setup_packet *sp = (struct setup_packet*)tmp;
				rimeaddr_t *neighbor_addr = sp->neighbors;

				sp->type = SETUP_PACKET;

				{
					rimeaddr_t addr;
					addr.u8[0] = 2;
					addr.u8[1] = 0;
					rimeaddr_copy(&sp->new_addr, &addr);
				}
				{
					struct coordinate coord = {1,0};
					sp->new_coord = coord;
				}

				sp->is_exit_node = 0;
				sp->num_neighbors = 2;

				{
					rimeaddr_t addr;

					addr.u8[0] = 1;
					addr.u8[1] = 0;
					rimeaddr_copy(neighbor_addr++, &addr);

					addr.u8[0] = 3;
					addr.u8[1] = 0;
					rimeaddr_copy(neighbor_addr++, &addr);
				}

				setup_parse(sp);

				ec_set_neighbors(&g_np.c, &g_np.ns);
				LOG("Im now node 2.0\n");
			} else if(strcmp(data, "3") == 0) {
				uint8_t tmp[SETUP_PACKET_SIZE+1*sizeof(rimeaddr_t)] = {0};
				struct setup_packet *sp = (struct setup_packet*)tmp;
				rimeaddr_t *neighbor_addr = sp->neighbors;

				sp->type = SETUP_PACKET;

				{
					rimeaddr_t addr;
					addr.u8[0] = 3;
					addr.u8[1] = 0;
					rimeaddr_copy(&sp->new_addr, &addr);
				}
				{
					struct coordinate coord = {2,0};
					sp->new_coord = coord;
				}

				sp->is_exit_node = 0;
				sp->num_neighbors = 1;

				{
					rimeaddr_t addr;

					addr.u8[0] = 2;
					addr.u8[1] = 0;
					rimeaddr_copy(neighbor_addr++, &addr);
				}

				setup_parse(sp);

				ec_set_neighbors(&g_np.c, &g_np.ns);
				LOG("Im now node 3.0\n");
			} else if(strcmp(data, "4") == 0) {
				LOG("Im now node 4.0\n");
			} else if(strcmp(data, "5") == 0) {
				LOG("Im now node 5.0\n");
			} else if(strcmp(data, "init") == 0) {
				rimeaddr_t addr = { {0xFE, 0xFE} };
				LOG("Initing\n");
				struct sensor_packet sp = {INITIALIZE_BEST_PATHS_PACKET};
				ec_recv(NULL, &addr, &addr, 0, 0, &sp, sizeof(struct sensor_packet));
			} else {
				LOG("ERROR: GOT MESSAGE: %s, len: %d\n", (char*)data, strlen(data));
			}
#endif
		} 
	//	else if (ev == sensor_timer_update) {
	//		if (!g_np.state.has_sensed_emergency && is_emergency()) {
	//			struct emergency_packet ep;
	//			ep.type = EMERGENCY_PACKET;
	//			ep.coord = coordinate_node;
	//			queue_buffer_push_front(&g_np.emergency_coords, &coordinate_node);

	//			ec_async_broadcast_ns(c, &rimeaddr_node_addr,
	//					&rimeaddr_node_addr, 0, g_np.seqno++, &ep,
	//					sizeof(struct emergency_packet));

	//			broadcast_best_path();

	//			/* Leader node should synchronize every 30 sec. New leader
	//			 * should be elected if we hit timeout of 60 sec. */
	//			ec_timesynch_network(c);

	//			leds_red(1);
	//			blinking_on();
	//			g_np.state.has_sensed_emergency = 1;
	//			g_np.state.is_aware_of_emergency = 1;
	//		} else if (g_np.state.is_aware_of_emergency) {
	//			if (sensor_critical_change()) {
	//				/* Critical sensor reading changes (e.g. if path gets
	//				 * congested) */
	//				broadcast_new_path();
	//			}
	//		}
	//	}
	}

	PROCESS_END();
}
