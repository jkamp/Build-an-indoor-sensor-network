#include "contiki.h"

#include "dev/leds.h"
#include "dev/button-sensor.h"
//#include "dev/light-sensor.h"
//#include "dev/sht11-sensor.h"
#include "sys/rtimer.h"
#include "net/rime/timesynch.h"
//#include "net/rime.h"
//#include "dev/cc2420.h"

#include "net/reliable_flood_neighbor_req.h"

#include "base/log.h"

#define FIREWSN_ANNOUNCE_CHANNEL 128
#define FIREWSN_ACK_CHANNEL 129

#define HOLD_BLINKING_STATUS_TIME 512
#define BLINKING_SEQUENCE_LENGTH 3


/****** PACKETS ******/
#define FIRE_PACKET 0
#define EXIT_NODE_PACKET 1
#define REQUEST_SHORTEST_EXIT_NODE_FROM_NEIGHBORS 2

struct fire_packet {
	uint8_t type;
};

struct exit_node_packet {
	uint8_t type;
	rtimer_clock_t blinking_started;
};

struct request_for_shortest_exit_node_packet {
	uint8_t type;
};
/****** END_PACKETS ******/

enum blinking_sequence_status {
	ON = 0, OFF_1 = 1, OFF_2 = 2
};

struct exit_node {
	uint8_t initialized;
	rimeaddr_t originator; /* the exit node's address */
	rimeaddr_t sender; /* our neighbor who relayed the exit node's path */
	uint8_t hops; /* 1 hop = you are neighbor with the exit node */
	rtimer_clock_t blinking_started; /* time when blinking started at
										originator (exit node) */
};

struct blinking_timer {
	struct rtimer rt;
	rtimer_clock_t next_wakeup;
	struct pt pt;
};

static struct rfnr_conn rfnr;

static struct {
	uint8_t is_exit_node;
	uint8_t has_sensed_fire;
	uint8_t is_aware_of_fire;
	uint8_t needs_to_announce_new_eni;
} node_status = {0};

static struct exit_node shortest_exit_node; 
static struct blinking_timer btimer;
static enum blinking_sequence_status bstatus = 
	(enum blinking_sequence_status)0;

static char 
blink(struct rtimer *rt, void *ptr) {
	PT_BEGIN(&btimer.pt);
	while(1) {
		switch(bstatus) {
			case ON:
				leds_blue(1);
				break;
			case OFF_1:
				leds_blue(0);
				break;
			case OFF_2:
				break;
		}
		bstatus = (bstatus+1) % BLINKING_SEQUENCE_LENGTH;
		btimer.next_wakeup += HOLD_BLINKING_STATUS_TIME;
		rtimer_set(&btimer.rt, btimer.next_wakeup, 0, 
				(rtimer_callback_t)blink, NULL);
		PT_YIELD(&btimer.pt);
	}
	PT_END(&btimer.pt);
}

static void
init_blinking() {
	PT_INIT(&btimer.pt);
	btimer.next_wakeup = shortest_exit_node.blinking_started;
	do {
		btimer.next_wakeup += HOLD_BLINKING_STATUS_TIME;
	} while (btimer.next_wakeup < timesynch_time());
	rtimer_set(&btimer.rt, btimer.next_wakeup, 0, (rtimer_callback_t)blink, NULL);
}

static void copy_and_setup_new_shortest_exit_node(const rimeaddr_t *originator, 
		const rimeaddr_t *sender, uint8_t hops, 
		rtimer_clock_t blinking_started) {

	rimeaddr_copy(&shortest_exit_node.originator, originator);
	rimeaddr_copy(&shortest_exit_node.sender, sender);
	shortest_exit_node.hops = hops;
	shortest_exit_node.blinking_started = blinking_started;
	init_blinking();
}

static rfnr_instructions_t
dupe_packet_recv(struct rfnr_conn *c, const rimeaddr_t *originator,
		const rimeaddr_t *sender, uint8_t hops, const void* data, 
		uint8_t data_length) {

	const struct fire_packet *p = (struct fire_packet*)data;
	const struct exit_node_packet *enp;

	switch(p->type) {
		case FIRE_PACKET:
			/* no need to sound the same fire alarm more than once */
			LOG("[NODE RECV DUPE] FP: o: %d.%d, f: %d.%d, Hops: %d\n",
					originator->u8[0], originator->u8[1], sender->u8[0],
					sender->u8[1], hops);
			return DROP_PACKET;

		case EXIT_NODE_PACKET:
			/* correction packet for exit node */
			enp =  (struct exit_node_packet*)p;
			LOG("[NODE RECV DUPE] ENP: o: %d.%d, f: %d.%d, Hops: %d\n",
					originator->u8[0], originator->u8[1], sender->u8[0],
					sender->u8[1], hops);

			if (hops < shortest_exit_node.hops) {
				copy_and_setup_new_shortest_exit_node(originator, sender, hops,
						enp->blinking_started);

				/* announce a shorter path */

				return FORWARD_PACKET | STORE_PACKET_FOR_FORWARDING;
			} else if (!rfnr_is_neighbor(c, &shortest_exit_node.sender)){ 
				/* check if the neighbor-classifier engine has updated its
				 * criterion for neighbors. It can be the case where a previous
				 * packet was sent when the neighbor classifier were not
				 * initialized enough and we thus can have the scenario where
				 * the closest packet is a packet from a now classified
				 * non-neighbor. Thus, we need to check if it still can be
				 * classified as a neighbor.*/

				struct request_for_shortest_exit_node_packet req = 
					{REQUEST_SHORTEST_EXIT_NODE_FROM_NEIGHBORS};
				LOG("Old neighbor is no longer neighbor, sending RENU packet.\n");

				rfnr_async_send(c, &req,
						sizeof(struct request_for_shortest_exit_node_packet));

				/* Old neighbor was wrongly classified as a neighbor previously.
				 * use new neighbor temporarly as shortest path. */
				copy_and_setup_new_shortest_exit_node(originator, sender, hops,
						enp->blinking_started);

				return FORWARD_PACKET | STORE_PACKET_FOR_FORWARDING;
			} else {
				/* dont announce a longer path */
				return DROP_PACKET;
			}
		default:
			return DROP_PACKET;
	}
}


static rfnr_instructions_t
packet_recv(struct rfnr_conn *c, const rimeaddr_t *originator, 
		const rimeaddr_t *sender, uint8_t hops, const void *data, 
		uint8_t data_len) {

	const struct fire_packet *p = (struct fire_packet*)data;
	const struct exit_node_packet *enp;

	switch(p->type) {
		case FIRE_PACKET:
			LOG("[NODE RECV] FP: o: %d.%d, f: %d.%d, Hops: %d\n",
					originator->u8[0], originator->u8[1], sender->u8[0],
					sender->u8[1], hops);

			if (!node_status.is_aware_of_fire) {
				leds_blue(1);
				node_status.is_aware_of_fire = 1;
			}

			if (node_status.is_exit_node) {
				struct exit_node_packet enp;
				rtimer_clock_t lights_start = timesynch_time();

				enp.type = EXIT_NODE_PACKET;
			   	enp.blinking_started = lights_start;
				rfnr_async_send(c, &enp, sizeof(struct exit_node_packet));

				copy_and_setup_new_shortest_exit_node(&rimeaddr_node_addr,
						&rimeaddr_node_addr, 0, lights_start);
			}

			return FORWARD_PACKET | STORE_PACKET_FOR_DUPE_CHECKS;

		case EXIT_NODE_PACKET:
			enp = (struct exit_node_packet*)p;
			LOG("[NODE RECV] ENP: o: %d.%d, f: %d.%d, Hops: %d\n",
					originator->u8[0], originator->u8[1], sender->u8[0],
					sender->u8[1], hops);
			if (!node_status.has_sensed_fire) {
				if (hops < shortest_exit_node.hops) {
					copy_and_setup_new_shortest_exit_node(originator, sender, hops,
							enp->blinking_started);

					/* announce a shorter path */
					return FORWARD_PACKET | STORE_PACKET_FOR_FORWARDING |
						STORE_PACKET_FOR_DUPE_CHECKS;
				} else if (!rfnr_is_neighbor(c, &shortest_exit_node.sender)){ 
					/* check if the neighbor-classifier engine has updated its
					 * criterion for neighbors. It can be the case where a previous
					 * packet was sent when the neighbor classifier were not
					 * initialized enough and we thus can have the scenario where
					 * the closest packet is a packet from a now classified
					 * non-neighbor. Thus, we need to check if it still can be
					 * classified as a neighbor.*/
					struct request_for_shortest_exit_node_packet req = 
						{REQUEST_SHORTEST_EXIT_NODE_FROM_NEIGHBORS};
					LOG("Old neighbor is no longer neighbor, sending RENU packet.\n");
					rfnr_async_send(c, &req,
							sizeof(struct request_for_shortest_exit_node_packet));

					/* Old neighbor was wrongly classified as a neighbor previously.
					 * use new neighbor temporarly as shortest path. */
					copy_and_setup_new_shortest_exit_node(originator, sender, hops,
							enp->blinking_started);

					return FORWARD_PACKET | STORE_PACKET_FOR_FORWARDING;
				}
			}

			/* no need to announce a longer route than we already have */
			/* and, we're not announcing a route through a burning path! */
			return DROP_PACKET;

		case REQUEST_SHORTEST_EXIT_NODE_FROM_NEIGHBORS:
			/* Neighbor requested our best path, which is the stored packet. 
			 * We drop the request because that is irrelevant to others. */
			LOG("[NODE RECV] RSENFM: o: %d.%d, f: %d.%d, Hops: %d\n",
					originator->u8[0], originator->u8[1], sender->u8[0],
					sender->u8[1], hops);
			if(shortest_exit_node.initialized)
				return DROP_PACKET | FORWARD_STORED_PACKET;

			return DROP_PACKET;
		default:
			LOG("[NODE RECV ERROR] UNKOWN PACKET: o: %d.%d, f: %d.%d, Hops: %d\n",
					originator->u8[0], originator->u8[1], sender->u8[0],
					sender->u8[1], hops);
			return DROP_PACKET;
	}
}

const static struct rfnr_callbacks rfnr_cb = {packet_recv,
	dupe_packet_recv};

PROCESS(fire_alarm_process, "FireWSN");
AUTOSTART_PROCESSES(&fire_alarm_process);

PROCESS_THREAD(fire_alarm_process, ev, data) {

	static struct etimer fire_check_timer;
	PROCESS_EXITHANDLER(rfnr_close(&rfnr));


	PROCESS_BEGIN();
	/* TODO: fix time synch. */
	//timesynch_init();
	rfnr_open(&rfnr, FIREWSN_ANNOUNCE_CHANNEL, FIREWSN_ACK_CHANNEL, &rfnr_cb);

	SENSORS_ACTIVATE(button_sensor);

	while(1) {

		etimer_set(&fire_check_timer, CLOCK_SECOND * 1);

		PROCESS_WAIT_EVENT();

		if (etimer_expired(&fire_check_timer)) {
			//SENSORS_ACTIVATE(light_sensor);
			//	SENSORS_ACTIVATE(sht11_sensor);
			//
			//	int photo = light_sensor.value(LIGHT_SENSOR_PHOTOSYNTHETIC);
			//	int solar = light_sensor.value(LIGHT_SENSOR_TOTAL_SOLAR);
			//	LOG("Light - Photosynthetic %d, Total solar: %d\n",
			//			photo,
			//			solar);
			//	if (photo > 250) {
			//		LOG("Broadcast fire SENDING\n");
			//		packetbuf_copyfrom(FIRE_MSG, FIRE_MSG_LENGTH);
			//		netflood_send(&exit_nodes_flood, 0);
			//	}
			//	//temp = -39.60 + 0.01*sht11_sensor.value(SHT11_SENSOR_TEMP);
			//	LOG("Temp %.1f, Humidity: %d\n",
			//			temp,
			//			sht11_sensor.value(SHT11_SENSOR_HUMIDITY));

			//SENSORS_DEACTIVATE(light_sensor);
			//	SENSORS_DEACTIVATE(sht11_sensor);

		} else if (ev == sensors_event) {
			if (data == &button_sensor) {
				static struct etimer exit_node_timer;
				etimer_set(&exit_node_timer, CLOCK_SECOND * 2);
				while(1) {
					PROCESS_WAIT_EVENT();
					if (etimer_expired(&exit_node_timer)) {
						//simulate node senses fire!
						if(!node_status.has_sensed_fire) {
							struct fire_packet f = {FIRE_PACKET};
							LOG("I SENSED FIRE!\n");
							leds_red(1);
							node_status.has_sensed_fire = 1;
							rfnr_async_send(&rfnr, &f,
									sizeof(struct fire_packet));
						//	packetbuf_copyfrom(FIRE_MSG, FIRE_MSG_LENGTH);
						//	netflood_send(&fire_flood, 0 /*seqno*/);
						} else {
							LOG("I already sensed fire...\n");
						}
						break;
					} else if(ev == sensors_event && data == &button_sensor) {
						node_status.is_exit_node ^= 1;
						if (node_status.is_exit_node) {
							LOG("I am now an EXIT node\n");
							leds_green(1);
						} else {
							LOG("I am now a REGULAR node\n");
							leds_green(0);
						}
						etimer_stop(&exit_node_timer);
						break;
					}
				}
			}
		}
	}
	PROCESS_END();
}
