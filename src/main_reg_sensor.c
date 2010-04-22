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

#define EMERGENCYNET_REGULAR_SENSOR_CHANNEL 128
#define BLINKING_SEQUENCE_LENGTH 4
#define BLINKING_SEQUENCE_SWITCH_TIME (1024)

/****** PACKETS ******/
enum {
	FIRE_PACKET = 0,
	RESYNCH_UPDATE = 1,
	APPLICATION_UPDATE_PACKET = 2
};

struct emergency_packet {
	uint8_t type;
};

/* TODO(johan): pragma pack push 1? */
struct fire_packet {
	uint8_t type;
	rtimer_clock_t blinking_started;
	rimeaddr_t nearest_exit_neighbor;
	uint8_t nearest_exit_hops;
};

enum blinking_sequence_status {
	ON = 0, OFF_1 = 1, OFF_2 = 2, OFF_3 = 3
};

struct blinking_timer {
	struct rtimer rt;
	struct ctimer synch_timer;
	rtimer_clock_t next_wakeup;
	//struct pt pt;
};

struct exit_path {
	rimeaddr_t neighbor;
	uint8_t hops; /* to exit */
	rtimer_clock_t blinking_started;
	int8_t points_to_us; /* is neighbor hop-count through us? */
	//int8_t burning; /* is neighbor burning? */
};

static int exit_path_cmp(const struct exit_path *lhs,
		const struct exit_path *rhs) {
	return rimeaddr_cmp(&lhs->neighbor, &rhs->neighbor) &&
		lhs->hops == rhs->hops;
}

struct routing_entry {
	uint16_t subnet_mask;
	rimeaddr_t next;
};

static struct ec emergency_connection;

struct node_properties {
	struct exit_path current_path;
	struct blinking_timer btimer;
	enum blinking_sequence_status bstatus;
	int8_t blink;
	int8_t on_fire;
	int8_t is_exit_node;
	uint8_t seqno;
	QUEUE_BUFFER(exit_paths, sizeof(struct exit_path), MAX_NEIGHBORS);
	//QUEUE_BUFFER(routing_entries, sizeof(struct routing_entry), MAX_NEIGHBORS);
};

static struct node_properties g_properties;

static void 
blink(struct rtimer *rt, void *ptr) {
	//PT_BEGIN(&g_properties.btimer.pt);
	//while(1) {
		switch(g_properties.bstatus) {
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
		g_properties.bstatus = (g_properties.bstatus+1) % BLINKING_SEQUENCE_LENGTH;
		g_properties.btimer.next_wakeup += BLINKING_SEQUENCE_SWITCH_TIME;
		if (g_properties.blink)
			rtimer_set(&g_properties.btimer.rt, g_properties.btimer.next_wakeup, 0, blink, NULL);
		else
			leds_blue(1);
		//PT_YIELD(&g_properties.btimer.pt);
	//}
	//PT_END(&g_properties.btimer.pt);
}

static void
update_blinking() {
	if (g_properties.blink) {
		if (g_properties.is_exit_node) {
			g_properties.bstatus = 0;
		} else {
			g_properties.bstatus = (g_properties.current_path.hops+1) % BLINKING_SEQUENCE_LENGTH;
		}

		{
			rtimer_clock_t now = timesynch_time();
			uint16_t blinking_sequences_next = (now / BLINKING_SEQUENCE_SWITCH_TIME)+1;
			//LOG("Update blinking time: %u\n", now);
		//	if (now < g_properties.current_path.blinking_started) {
		//		now += (USHRT_MAX - g_properties.current_path.blinking_started);
		//	}
			//LOG("MY TIME: %d\n", now);

			g_properties.btimer.next_wakeup = timesynch_time_to_rtimer(blinking_sequences_next*BLINKING_SEQUENCE_SWITCH_TIME);
			rtimer_set(&g_properties.btimer.rt, g_properties.btimer.next_wakeup, 0, /*(rtimer_callback_t)*/blink, NULL);

			g_properties.bstatus = (g_properties.bstatus+blinking_sequences_next) % BLINKING_SEQUENCE_LENGTH;
		}
	}
}


static void 
resynch_blinking(void *unused) {
	struct emergency_packet r = {RESYNCH_UPDATE};
	ec_async_broadcast(&emergency_connection, &rimeaddr_node_addr, &rimeaddr_node_addr,
			0, g_properties.seqno++, &r, sizeof(struct emergency_packet));
	ctimer_set(&g_properties.btimer.synch_timer, 60*CLOCK_SECOND, resynch_blinking, NULL);
}

static void
init_blinking() {
	//PT_INIT(&g_properties.btimer.pt);
	//TRACE("Init blinking...\n");
	//g_properties.btimer.next_wakeup = g_properties.current_path.blinking_started;

	g_properties.blink = 1;
	update_blinking();
	if (g_properties.is_exit_node)
		resynch_blinking(NULL);


//	do {
//		g_properties.btimer.next_wakeup += BLINKING_SEQUENCE_SWITCH_TIME;
//	} while (g_properties.btimer.next_wakeup < timesynch_time());
}


//static void blink(void* unused) {
//	switch(g_properties.bstatus) {
//		case ON:
//			leds_blue(1);
//			break;
//		case OFF_1:
//			leds_blue(0);
//			break;
//		case OFF_2:
//			break;
//	}
//
//	if (g_properties.blink) {
//		ctimer_reset(&g_properties.btimer.ct);
//		g_properties.bstatus = (g_properties.bstatus+1) % BLINKING_SEQUENCE_LENGTH;
//	} else {
//		leds_blue(1);
//	}
//}
//
//static void blink_start(void *unused) {
//	ctimer_set(&g_properties.btimer.ct,
//			BLINKING_SEQUENCE_SWITCH_TIME, blink,
//			NULL);
//	g_properties.bstatus = (g_properties.bstatus+1) % BLINKING_SEQUENCE_LENGTH;
//}
//
////static struct rtimer blinking_init_timer;
//static void init_blinking() {
//	rtimer_clock_t next_wakeup = g_properties.current_path.blinking_started;
//	g_properties.bstatus = (g_properties.current_path.hops+1) % BLINKING_SEQUENCE_LENGTH;
//	do {
//		next_wakeup += RTIMER_SECOND;
//		g_properties.bstatus = (g_properties.bstatus+1) % BLINKING_SEQUENCE_LENGTH;
//	} while (next_wakeup < RTIMER_NOW());
//
//	//rtimer_set(&blinking_init_timer, next_wakeup, 0, blink_start, NULL);
//	ctimer_set(&g_properties.btimer.ct,
//			(next_wakeup-RTIMER_NOW())*CLOCK_SECOND/RTIMER_SECOND, blink_start,
//			NULL);
//	g_properties.blink = 1;
//}

static inline void
cancel_blinking() {
	//ctimer_stop(&g_properties.btimer.synch_timer);
	g_properties.blink = 0;
}

static inline void
set_exit_node(int on) {
	g_properties.is_exit_node = on;
	if (on) {
		leds_green(1);
	}
}

static int 
exit_path_to_addr_cmp(const void *queued_item, const void *supplied_item) {
	const struct exit_path *ep = (struct exit_path*)queued_item;
	const rimeaddr_t *si = (rimeaddr_t*)supplied_item;
	return rimeaddr_cmp(&ep->neighbor, si);
}

static struct exit_path*
update_and_get_nearest_path(const struct fire_packet *fp, const rimeaddr_t *sender) {

	struct exit_path *nearest = NULL;
	struct exit_path *ep = (struct exit_path*)
		queue_buffer_find(&g_properties.exit_paths, sender,
				exit_path_to_addr_cmp);
	ASSERT(ep != NULL);
	ep->hops = fp->nearest_exit_hops;
	ep->blinking_started = fp->blinking_started;

	if (rimeaddr_cmp(&fp->nearest_exit_neighbor, &rimeaddr_node_addr)) {
		/* Neighbor node is thinking its nearest path is through us. */
		ep->points_to_us = 1;
	} else {
		ep->points_to_us = 0;
	}

	{
		uint8_t least_hops = 0xFF;
		struct exit_path *i;
		for (i = (struct exit_path*)queue_buffer_begin(&g_properties.exit_paths); 
				i != NULL;
				i = (struct exit_path*)queue_buffer_next(&g_properties.exit_paths)) {
			LOG("Neighbor: %d.%d, Points-to-us: %d, hops:%d\n", 
					i->neighbor.u8[0], i->neighbor.u8[1],
					i->points_to_us, i->hops);
			if (!i->points_to_us && i->hops < least_hops) {
				least_hops = i->hops;
				nearest = i;
			}
		}
	}

	return nearest;
}

static void process_fire_packet(struct ec *c, const struct fire_packet *fp,
		const rimeaddr_t *originator, const rimeaddr_t *sender, uint8_t hops,
		uint8_t seqno) {

	const struct exit_path *nearest = update_and_get_nearest_path(fp, sender);

	if (nearest != NULL) {
		if(!exit_path_cmp(&g_properties.current_path, nearest)) {
			/* Update in the exit path. Broadcast needed! */
			struct fire_packet new_fp;
			new_fp.type = FIRE_PACKET;
			new_fp.blinking_started = 
				nearest->blinking_started == 0 ? fp->blinking_started :
				nearest->blinking_started;

			rimeaddr_copy(&new_fp.nearest_exit_neighbor, &nearest->neighbor);
			new_fp.nearest_exit_hops = nearest->hops+1;
			g_properties.current_path = *nearest;
			init_blinking();
			ec_async_broadcast(c, originator, &rimeaddr_node_addr,
					hops+1, seqno, &new_fp, sizeof(struct fire_packet));
		}
	} else {
		/* Every neighbor either points to us or does not know a way out. Which
		 * means that we have no idea where an exit is. */
		struct fire_packet new_fp;
		new_fp.type = FIRE_PACKET;
		new_fp.blinking_started = fp->blinking_started;
	   	new_fp.nearest_exit_hops = 0xFF;
		rimeaddr_copy(&new_fp.nearest_exit_neighbor, &rimeaddr_null);

		rimeaddr_copy(&g_properties.current_path.neighbor, &rimeaddr_null);

		cancel_blinking();
		ec_async_broadcast(c, originator, &rimeaddr_node_addr,
				hops+1, seqno, &new_fp, sizeof(struct fire_packet));
	}
}

static void ec_recv_dupe(struct ec *c, const rimeaddr_t *originator, 
		const rimeaddr_t *sender, uint8_t hops, uint8_t seqno, 
		const void *data, uint8_t data_len) {
	const struct emergency_packet *p = (struct emergency_packet*)data;

	update_blinking();

	switch(p->type) {
		case FIRE_PACKET:
			{
				const struct fire_packet *fp = (struct fire_packet*)p;
				LOG("[FP RECV DUPE]: type: %d, bstart: %d, nearest_en: %d.%d, "
						"nereast_exit_hops: %d\n",
						fp->type, fp->blinking_started,
						fp->nearest_exit_neighbor.u8[0],
						fp->nearest_exit_neighbor.u8[1],
						fp->nearest_exit_hops);
				if (g_properties.is_exit_node) {
					/* neighbors already know our best path. dont announce it
					 * again. */
				} else if (g_properties.on_fire) {
					const struct exit_path *nearest =
						update_and_get_nearest_path(fp, sender);

					if (nearest != NULL) {
						g_properties.current_path = *nearest;
						init_blinking();
					}
				} else {
					process_fire_packet(c, fp, originator, sender, hops, seqno);
				}
			}
			break;
		case RESYNCH_UPDATE:
			break;
		case APPLICATION_UPDATE_PACKET:
			ASSERT(0);
			break;
		default:
			LOG("Unknown application packet type\n");
	}

}

static void ec_recv(struct ec *c, const rimeaddr_t *originator, 
		const rimeaddr_t *sender, uint8_t hops, uint8_t seqno, 
		const void *data, uint8_t data_len) {
	const struct emergency_packet *p = (struct emergency_packet*)data;

	update_blinking();

	switch(p->type) {
		case FIRE_PACKET:
			{
				const struct fire_packet *fp = (struct fire_packet*)p;
				LOG("FP RECV: type: %d, bstart: %d, nearest_en: %d.%d, "
						"nereast_exit_hops: %d\n",
						fp->type, fp->blinking_started,
						fp->nearest_exit_neighbor.u8[0],
						fp->nearest_exit_neighbor.u8[1],
						fp->nearest_exit_hops);

				if (g_properties.is_exit_node) {
					struct fire_packet new_fp;
					rtimer_clock_t my_time = fp->blinking_started;

					new_fp.type = FIRE_PACKET;
					new_fp.blinking_started = my_time;
					rimeaddr_copy(&new_fp.nearest_exit_neighbor, &rimeaddr_node_addr);
					new_fp.nearest_exit_hops = 0;
					g_properties.current_path.hops = 0;
					g_properties.current_path.blinking_started = my_time;
					init_blinking();
					ec_async_broadcast(c, originator, &rimeaddr_node_addr,
							hops+1, seqno, &new_fp, sizeof(struct fire_packet));
				} else if (g_properties.on_fire) {
					/* drop packet. */
					const struct exit_path *nearest =
						update_and_get_nearest_path(fp, sender);

					if (nearest != NULL) {
						g_properties.current_path = *nearest;
						init_blinking();
					} else {
						cancel_blinking();
					}
				} else {
					process_fire_packet(c, fp, originator, sender, hops, seqno);
				}
			}
			break;
		case RESYNCH_UPDATE:
			{
				struct emergency_packet r = {RESYNCH_UPDATE};
				ec_async_broadcast(c, originator, &rimeaddr_node_addr,
						hops+1, seqno, &r, sizeof(struct emergency_packet));
			}
			break;
		case APPLICATION_UPDATE_PACKET:
			ASSERT(0);
			break;
		default:
			LOG("Unknown application packet type\n");
			ASSERT(0);
	}
}

const static struct ec_callbacks ec_cb = {ec_recv, ec_recv_dupe};


PROCESS(fire_process, "FireWSN");
AUTOSTART_PROCESSES(&fire_process);

PROCESS_THREAD(fire_process, ev, data) {

	//static struct etimer fire_check_timer;
	PROCESS_EXITHANDLER(ec_close(&emergency_connection));

	PROCESS_BEGIN();
	g_properties.seqno = 0;
	QUEUE_BUFFER_INIT_WITH_STRUCT(&g_properties, exit_paths, 
			sizeof(struct exit_path), MAX_NEIGHBORS);
	//	QUEUE_BUFFER_INIT_WITH_STRUCT(&g_properties, routing_entries, 
	//			sizeof(struct exit_path), MAX_NEIGHBORS);
	ec_open(&emergency_connection, EMERGENCYNET_REGULAR_SENSOR_CHANNEL, &ec_cb);
	SENSORS_ACTIVATE(button_sensor);

	while(1) {
		//etimer_set(&fire_check_timer, CLOCK_SECOND * 1);
		PROCESS_WAIT_EVENT();

		if (ev == sensors_event) {
			if (data == &button_sensor) {
				struct fire_packet fp;
				fp.type = FIRE_PACKET;
				fp.blinking_started = RTIMER_NOW();
				rimeaddr_copy(&fp.nearest_exit_neighbor, &rimeaddr_null);
			   	fp.nearest_exit_hops = 0xFF;
				rimeaddr_copy(&g_properties.current_path.neighbor, &rimeaddr_null);
				LOG("I SENSED FIRE!\n");
				cancel_blinking();
				ec_async_broadcast(&emergency_connection, &rimeaddr_node_addr,
						&rimeaddr_node_addr, 0, g_properties.seqno++, &fp,
						sizeof(struct fire_packet));
				leds_red(1);
				g_properties.on_fire = 1;
			} 
		} else if (ev == serial_line_event_message && data != NULL) {
			if (strcmp(data, "1") == 0) {
				struct neighbors ns;
				struct exit_path ep;
				memset(&ep, 0, sizeof(struct exit_path));
				neighbors_init(&ns);

			//	ep.neighbor.u8[0] = 1;
			//	ep.neighbor.u8[1] = 0;
			//	ep.hops = 0;
				//rimeaddr_set_node_addr(&ep.neighbor);
				//queue_buffer_push_front(&g_properties.exit_paths, &ep);
				{ 
					rimeaddr_t addr;
					addr.u8[0] = 1;
					addr.u8[1] = 0;
					rimeaddr_set_node_addr(&addr);
				}

				ep.neighbor.u8[0] = 2;
				ep.neighbor.u8[1] = 0;
				ep.hops = 1;
				neighbors_add(&ns, &ep.neighbor);
				queue_buffer_push_front(&g_properties.exit_paths, &ep);
				ec_update_neighbors(&emergency_connection, &ns);

				set_exit_node(1);

				LOG("Im now node 1.0 (exit node)\n");
			} else if(strcmp(data, "2") == 0) {
				struct neighbors ns;
				struct exit_path ep;
				memset(&ep, 0, sizeof(struct exit_path));
				neighbors_init(&ns);

				{ 
					rimeaddr_t addr;
					addr.u8[0] = 2;
					addr.u8[1] = 0;
					rimeaddr_set_node_addr(&addr);
				}

				ep.neighbor.u8[0] = 1;
				ep.neighbor.u8[1] = 0;
				ep.hops = 0;
				neighbors_add(&ns, &ep.neighbor);
				queue_buffer_push_front(&g_properties.exit_paths, &ep);

				ep.neighbor.u8[0] = 3;
				ep.neighbor.u8[1] = 0;
				ep.hops = 2;
				neighbors_add(&ns, &ep.neighbor);
				queue_buffer_push_front(&g_properties.exit_paths, &ep);

				ec_update_neighbors(&emergency_connection, &ns);
				LOG("Im now node 2.0\n");
			} else if(strcmp(data, "3") == 0) {
				struct neighbors ns;
				struct exit_path ep;
				memset(&ep, 0, sizeof(struct exit_path));
				neighbors_init(&ns);

				{
					rimeaddr_t addr;
					addr.u8[0] = 3;
					addr.u8[1] = 0;
					rimeaddr_set_node_addr(&addr);
				}

				ep.neighbor.u8[0] = 2;
				ep.neighbor.u8[1] = 0;
				ep.hops = 1;
				neighbors_add(&ns, &ep.neighbor);
				queue_buffer_push_front(&g_properties.exit_paths, &ep);

				ep.neighbor.u8[0] = 4;
				ep.neighbor.u8[1] = 0;
				ep.hops = 3;
				neighbors_add(&ns, &ep.neighbor);
				queue_buffer_push_front(&g_properties.exit_paths, &ep);

				ec_update_neighbors(&emergency_connection, &ns);
				LOG("Im now node 3.0\n");
			} else if(strcmp(data, "4") == 0) {
				struct neighbors ns;
				struct exit_path ep;
				memset(&ep, 0, sizeof(struct exit_path));
				neighbors_init(&ns);

				{
					rimeaddr_t addr;
					addr.u8[0] = 4;
					addr.u8[1] = 0;
					rimeaddr_set_node_addr(&addr);
				}

				ep.neighbor.u8[0] = 3;
				ep.neighbor.u8[1] = 0;
				ep.hops = 3;
				neighbors_add(&ns, &ep.neighbor);
				queue_buffer_push_front(&g_properties.exit_paths, &ep);

				ec_update_neighbors(&emergency_connection, &ns);
				LOG("Im now node 4.0\n");
			} else if(strcmp(data, "5") == 0) {
			} else {
				LOG("ERROR: GOT MESSAGE: %s, len: %d\n", (char*)data, strlen(data));
			}
		}
	}

	PROCESS_END();
}
