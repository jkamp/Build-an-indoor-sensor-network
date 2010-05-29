#include <stdlib.h>

#include "net/rime/timesynch.h"
#include "net/rime.h"
#include "dev/cc2420.h"

#include "emergency_net/packet.h"

#include "emergency_net/timesynch_gluer.h"

#include "base/log.h"

#define MAX_OFFSETS 12

static rtimer_clock_t offset;
static rtimer_clock_t offsets[MAX_OFFSETS];
static int offset_index;


//int offset_cmp(const void *lhs, const void *rhs) {
//	int *l = (int*)lhs;
//	int *r = (int*)rhs;
//
//	if (*l < *r) {
//		return -1;
//	} else if (*l > *r) {
//		return 1;
//	} else {
//		return 0;
//	}
//}

void bs(rtimer_clock_t a[], int array_size)
{
	int i, j, temp;
	for (i = 0; i < (array_size - 1); ++i)
	{
		for (j = 0; j < array_size - 1 - i; ++j )
		{
			if (a[j] > a[j+1])
			{
				temp = a[j+1];
				a[j+1] = a[j];
				a[j] = temp;
			}
		}
	}
}



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
	LOG("Adjusting offset: before: %u", offset);
	if (offset_index < MAX_OFFSETS) {
		offsets[offset_index++] = offset + authoritative_time - local_time;
		//qsort(offsets, offset_index, sizeof(rtimer_clock_t), offset_cmp);
		bs(offsets, offset_index);
	}

	offset = offsets[offset_index/2];
	//offset = offset + authoritative_time - local_time;
	LOG(", after: %u\n", offset);
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
					offset_index = 0;
					adjust_offset(cc2420_time_of_departure, cc2420_time_of_arrival);
				} else if(cc2420_authority_level_of_sender == authority_level) {
					if (p->hdr.seqno > authority_seqno || p->hdr.seqno < authority_seqno/8 /*overflow*/) {
						offset_index = 0;
						adjust_offset(cc2420_time_of_departure, cc2420_time_of_arrival);
					} else if (p->hdr.seqno == authority_seqno) {
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
	offset_index = 0;
	TRACE("Timesynch initialized.\n");
}
/*---------------------------------------------------------------------------*/
