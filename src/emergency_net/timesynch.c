#include "net/rime/timesynch.h"
#include "net/rime.h"
#include "dev/cc2420.h"

#include "emergency_net/packet.h"

#include "emergency_net/timesynch_gluer.h"

#include "base/log.h"

static rtimer_clock_t offset;

/*---------------------------------------------------------------------------*/
	int
timesynch_authority_level(void)
{
	return authority_level;
}
/*---------------------------------------------------------------------------*/
/*	void
timesynch_set_authority_level(int level)
{
	seqno = 0;
	authority_level = level;
}*/
/*---------------------------------------------------------------------------*/
	rtimer_clock_t
timesynch_time(void)
{
	return RTIMER_NOW() + offset;
}
/*---------------------------------------------------------------------------*/
	rtimer_clock_t
timesynch_time_to_rtimer(rtimer_clock_t synched_time)
{
	return synched_time - offset;
}
/*---------------------------------------------------------------------------*/
	rtimer_clock_t
timesynch_rtimer_to_time(rtimer_clock_t rtimer_time)
{
	return rtimer_time + offset;
}
/*---------------------------------------------------------------------------*/
	rtimer_clock_t
timesynch_offset(void)
{
	return offset;
}
/*---------------------------------------------------------------------------*/
	static void
adjust_offset(rtimer_clock_t authoritative_time, rtimer_clock_t local_time)
{
	LOG("Adjusting offset: before: %d", offset);
	offset = offset + authoritative_time - local_time;
	LOG(", after: %d\n", offset);
}
/*---------------------------------------------------------------------------*/
	static void
incoming_packet(void)
{
	if(packetbuf_totlen() != 0) {
		const uint8_t *channel = (uint8_t*)packetbuf_dataptr();
		if(*channel == timesynch_channel) {
			const struct packet *p = (struct packet*)(((char*)packetbuf_dataptr())+2);
			if(IS_PACKET_FLAG_SET(p, TIMESYNCH)) {
				if(cc2420_authority_level_of_sender < authority_level) {
					adjust_offset(cc2420_time_of_departure, cc2420_time_of_arrival);
				} else if(cc2420_authority_level_of_sender == authority_level) {
					if (p->hdr.seqno > authority_seqno || p->hdr.seqno < authority_seqno/8 /*overflow*/) {
						adjust_offset(cc2420_time_of_departure, cc2420_time_of_arrival);
					}
				}
			}
		}
	}
}

/*---------------------------------------------------------------------------*/
RIME_SNIFFER(sniffer, incoming_packet, NULL);
/*---------------------------------------------------------------------------*/
	void
timesynch_init(void)
{
	rime_sniffer_add(&sniffer);
	TRACE("Timesynch initialized.\n");
}
/*---------------------------------------------------------------------------*/
