#include "packet.h"

#include "base/log.h"

int packet_to_packet_cmp(const void *queued_item, const void *supplied_item) {
	const struct packet *l = (struct packet*)queued_item;
	const struct packet *r = (struct packet*)supplied_item;

	return rimeaddr_cmp(&l->hdr.originator, &r->hdr.originator) && 
		l->hdr.originator_seqno == r->hdr.originator_seqno;
}

int slim_packet_to_packet_cmp(const void *queued_item, const void *supplied_item) {
	const struct slim_packet *l = (struct slim_packet*)queued_item;
	const struct packet *r = (struct packet*)supplied_item;

	return rimeaddr_cmp(&l->originator, &r->hdr.originator) && 
		l->originator_seqno == r->hdr.originator_seqno;
}
