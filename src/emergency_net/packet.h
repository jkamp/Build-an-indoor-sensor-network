#ifndef _PACKET_H_
#define _PACKET_H_

#include "net/rime/rimeaddr.h"

#define MAX_PACKET_SIZE 24

#define IS_PACKET_FLAG_SET(packet, flag) (((packet)->hdr.type_and_flags & (flag)) == (flag))

#define PACKET_TYPE(packet) (((packet)->hdr.type_and_flags & 0xC0))

#define PACKET_HDR_SIZE (sizeof(struct packet)-sizeof(uint8_t))
#define BROADCAST_PACKET_HDR_SIZE (sizeof(struct broadcast_packet)-sizeof(uint8_t))
#define MULTICAST_PACKET_HDR_SIZE (sizeof(struct multicast_packet)-sizeof(uint8_t))
#define UNICAST_PACKET_HDR_SIZE (sizeof(struct unicast_packet)-sizeof(uint8_t))
#define SLIM_PACKET_SIZE (sizeof(struct slim_packet))

#define DEBUG_PACKET(p) LOG("type:%x, hops: %d, o:%d.%d, s:%d.%d, seqno:%d\n", \
			(p)->hdr.type_and_flags, (p)->hdr.hops,  \
			(p)->hdr.originator.u8[0], (p)->hdr.originator.u8[1], \
			(p)->hdr.sender.u8[0], (p)->hdr.sender.u8[1], (p)->hdr.seqno)

enum packet_type {
	BROADCAST = 0x00,
	MULTICAST = 0x40,
	UNICAST = 0x80
};

enum packet_flags {
	ACK = 0x20,
	TIMESYNCH = 0x10
};

struct header {
	/* 2 bits type, 1 bit ack, 1 bit timesynch, 4 reserved */
	uint8_t type_and_flags;
	uint8_t hops;
	rimeaddr_t originator; /* original sender (origin) */
	rimeaddr_t sender; /* forwarder */
	uint8_t seqno;  /* originator seqno */
};

struct packet {
	struct header hdr;
	uint8_t data[1];
};

struct broadcast_packet {
	struct header hdr;
	uint8_t data[1];
};

struct multicast_packet {
	struct header hdr;
	uint8_t num_ids;
	uint8_t data[1];
};

struct unicast_packet {
	struct header hdr;
	rimeaddr_t destination;
	uint8_t data[1];
};

/*used for long term storing for dupe checking */
struct slim_packet {
	rimeaddr_t originator;
	uint8_t seqno;
};

void init_packet(struct packet *p, uint8_t type_and_flags,
	   	uint8_t hops, const rimeaddr_t *originator,
		const rimeaddr_t *sender, uint8_t seqno);

void init_broadcast_packet(struct broadcast_packet *bp, enum packet_flags flags, 
		uint8_t hops, const rimeaddr_t *originator, const rimeaddr_t *sender,
		uint8_t seqno);

void init_multicast_packet(struct multicast_packet *mp, 
		enum packet_flags flags, uint8_t hops, const rimeaddr_t *originator,
		const rimeaddr_t *sender, uint8_t seqno, uint8_t num_ids);

void init_unicast_packet(struct unicast_packet *up, enum packet_flags flags,
		uint8_t hops, const rimeaddr_t *originator, const rimeaddr_t *sender,
		uint8_t seqno, const rimeaddr_t *destination);

/* Compares two packets, if originator addr and originator seqno are equal. */
int originator_seqno_cmp(const void *queued_item, const void *supplied_item);

int unicast_packet_cmp(const void *queued_item, const void *supplied_item);

int slim_packet_to_packet_cmp(const void *queued_item, const void *supplied_item);
#endif
