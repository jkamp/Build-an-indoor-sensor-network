#include "packet.h"

#include "string.h"

#include "base/log.h"

int originator_seqno_cmp(const void *queued_item, const void *supplied_item) {
	const struct packet *l = (struct packet*)queued_item;
	const struct packet *r = (struct packet*)supplied_item;

	return rimeaddr_cmp(&l->hdr.originator, &r->hdr.originator) && 
		l->hdr.seqno == r->hdr.seqno;
}

int unicast_packet_cmp(const void *queued_item, const void *supplied_item) {

	return memcmp(queued_item, supplied_item, UNICAST_PACKET_HDR_SIZE) == 0;
}

int slim_packet_to_packet_cmp(const void *queued_item, const void *supplied_item) {
	const struct slim_packet *l = (struct slim_packet*)queued_item;
	const struct packet *r = (struct packet*)supplied_item;

	return rimeaddr_cmp(&l->originator, &r->hdr.originator) && 
		l->seqno == r->hdr.seqno;
}

void init_packet(struct packet *p, uint8_t type_and_flags,
	   	uint8_t hops, const rimeaddr_t *originator,
		const rimeaddr_t *sender, uint8_t seqno) {

	p->hdr.type_and_flags = type_and_flags;
	p->hdr.hops = hops;
	rimeaddr_copy(&p->hdr.originator, originator);
	rimeaddr_copy(&p->hdr.sender, sender);
	p->hdr.seqno = seqno;

}

void init_broadcast_packet(struct broadcast_packet *bp, enum packet_flags flags, 
		uint8_t hops, const rimeaddr_t *originator, const rimeaddr_t *sender,
		uint8_t seqno) {
	init_packet((struct packet*)bp, BROADCAST|flags, hops, originator, sender, seqno);
}

void init_multicast_packet(struct multicast_packet *mp, 
		enum packet_flags flags, uint8_t hops, const rimeaddr_t *originator,
		const rimeaddr_t *sender, uint8_t seqno, uint8_t num_ids) {

	init_packet((struct packet*)mp, MULTICAST|flags, hops, originator, sender, seqno);
	mp->num_ids = num_ids;
}

void init_unicast_packet(struct unicast_packet *up, enum packet_flags flags,
		uint8_t hops, const rimeaddr_t *originator, const rimeaddr_t *sender,
		uint8_t seqno, const rimeaddr_t *destination) {
	init_packet((struct packet*)up, UNICAST|flags, hops, originator, sender, seqno);
	rimeaddr_copy(&up->destination, destination);
}
