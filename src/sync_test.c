#include "contiki.h"

#include "dev/button-sensor.h"
#include "dev/cc2420.h"

#include "rime/abc.h"
#include "rime/timesynch.h"

#include "base/log.h"

	static void
abc_recv(struct abc_conn *c)
{
	LOG("HEJ HEJ\n");
	LOG("Msg recv T:%u, Ts: %u\n", RTIMER_NOW(), timesynch_time());
}

static const struct abc_callbacks abc_call = {abc_recv};
static struct abc_conn abc;

/*---------------------------------------------------------------------------*/
PROCESS(fire_process, "timesync_test");
AUTOSTART_PROCESSES(&fire_process);

PROCESS_THREAD(fire_process, ev, data) {
	//static struct etimer et;

	PROCESS_EXITHANDLER(abc_close(&abc););

	PROCESS_BEGIN();
//	timesynch_init();
//	timesynch_set_authority_level(rimeaddr_node_addr.u8[0]);

	abc_open(&abc, 128, &abc_call);
	SENSORS_ACTIVATE(button_sensor);

	while(1) {

		PROCESS_WAIT_EVENT();

		if (ev == sensors_event) {
			if (data == &button_sensor) {
				packetbuf_copyfrom("Hello", 6);
				abc_send(&abc);
				LOG("abc message sent\n");
			}
		}
		//etimer_set(&et, CLOCK_SECOND * 2 + random_rand() % (CLOCK_SECOND * 2));
	}

	PROCESS_END();
}
