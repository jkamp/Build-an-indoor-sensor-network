#ifndef _PACKET_H_
#define _PACKET_H_

#include "net/rime/rimeaddr.h"

#define MAX_DATA_SIZE 4

#define IS_DATA(packet) (((packet)->hdr.type_and_hops & 0x80) == 0)
#define IS_ACK(packet) (((packet)->hdr.type_and_hops & 0x80) == 0x80)

#define SET_DATA(packet) ((packet)->hdr.type_and_hops &= 0x7F)
#define SET_ACK(packet) ((packet)->hdr.type_and_hops |= 0x80)

#define NUM_HOPS(packet) ((packet)->hdr.type_and_hops & 0x7F)
#define SET_HOPS(packet, hops) ((packet)->hdr.type_and_hops = \
		((packet)->hdr.type_and_hops & 0x80) | (0x7F & (hops)))

#define HEADER_SIZE (sizeof(struct header))
/*#define PACKET_SIZE (sizeof(struct packet))*/
/*#define DATA_PACKET_SIZE (sizeof(struct data_packet)-sizeof(uint8_t))*/
#define SLIM_PACKET_SIZE (sizeof(struct slim_packet))
#define ACK_PACKET_SIZE (sizeof(struct ack_packet))

#define DEBUG_PACKET(p) LOG("[PCKT DBG] typehops:%x, s:%d.%d, o:%d.%d, seqno:%d\n", \
			p->hdr.type_and_hops, p->hdr.sender.u8[0], p->hdr.sender.u8[1], \
			p->hdr.originator.u8[0], p->hdr.originator.u8[1], \
			p->hdr.originator_seqno)

struct header {
	/* 1bit type, 7 bit number of hops */
	uint8_t type_and_hops;
	rimeaddr_t sender; /* forwarder */
	rimeaddr_t originator; /* original sender (origin) */
	uint8_t originator_seqno; 
};

struct packet {
	struct header hdr;
};

struct data_packet {
	struct header hdr;
	/* stupid c89 doesnt allow flexible arrays */
	uint8_t data[1];
};

struct ack_packet {
	struct header hdr;
	rimeaddr_t destination; 
};

/*used for long term storing*/
struct slim_packet {
	rimeaddr_t originator;
	uint8_t originator_seqno;
};

/* Compares two packets, if originator addr and originator seqno is equal. */
int packet_to_packet_cmp(const void *queued_item, const void *supplied_item);
int slim_packet_to_packet_cmp(const void *queued_item, const void *supplied_item);

#endif
