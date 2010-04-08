
#include "string.h"

#include "net/packet.h"

#include "base/queue_buffer.h"

#include "base/log.h"


struct test_s {
	char guard1;
	char guard2;
	QUEUE_BUFFER(qb, sizeof(struct packet), 8);
	char guard3;
	char guard4;
};

int main(void) {
	struct test_s s;
	s.guard1 = 'A';
	s.guard2 = 'B';
	s.guard3 = 'C';
	s.guard4 = 'D';

	QUEUE_BUFFER_INIT_WITH_STRUCT(&s, qb, sizeof(struct packet), 8);

	ASSERT(queue_buffer_size(&s.qb) == 0);
	ASSERT(queue_buffer_max_size(&s.qb) == 8);
	{
		struct packet p = { {7, {{8, 9}}, {{10, 11}}, 12} };
		struct packet *pp = (struct packet*)queue_buffer_push_front(&s.qb, &p);
		ASSERT(memcmp(&p, pp, sizeof(struct packet)) == 0);

		struct packet *ppp = (struct packet*)queue_buffer_find(&s.qb, &p, packet_to_packet_cmp);
		ASSERT(ppp == pp);
		queue_buffer_free(&s.qb, ppp);
		ppp = (struct packet*)queue_buffer_find(&s.qb, &p, packet_to_packet_cmp);
		ASSERT(ppp == NULL);
	}
	{
		struct packet p = { {1, {{2, 3}}, {{4, 5}}, 6} };
		struct packet *pp = (struct packet*)queue_buffer_push_front(&s.qb, &p);

	}

//	ASSERT(packet_buffer_has_packet_for_sending(&s.buf) == 0);
//	ASSERT(packet_buffer_get_packet_for_sending(&s.buf) == NULL);
//
//	{
//		struct packet p = { {1, {{2, 3}}, {{4, 5}}, 6} };
//		struct buffered_packet *bp = packet_buffer_packet(&s.buf, &p, "joha", 4, 1);
//		struct packet *pp = packet_buffer_get_packet(bp);
//
//		ASSERT(memcmp(pp, &p, sizeof(struct packet)) == 0);
//		ASSERT(packet_buffer_data_len(bp) == 4);
//		ASSERT(packet_buffer_times_acked(bp) == 0);
//		ASSERT(packet_buffer_times_sent(bp) == 0);
//		ASSERT(memcmp("joha", packet_buffer_get_data(bp), packet_buffer_data_len(bp)) == 0);
//		ASSERT(packet_buffer_find_packet(&s.buf, &p) == bp);
//
//		ASSERT(packet_buffer_get_packet_for_sending(&s.buf) == bp);
//	}
//	{
//		struct packet p = { {3, {{4, 5}}, {{6,7}},8} };
//		struct buffered_packet *bp = packet_buffer_packet(&s.buf, &p, "joH", 3, 2);
//		struct packet *pp = packet_buffer_get_packet(bp);
//
//		ASSERT(memcmp(pp, &p, sizeof(struct packet)) == 0);
//		ASSERT(packet_buffer_data_len(bp) == 3);
//		ASSERT(packet_buffer_times_acked(bp) == 0);
//		ASSERT(packet_buffer_times_sent(bp) == 0);
//		ASSERT(memcmp("joH", packet_buffer_get_data(bp), packet_buffer_data_len(bp)) == 0);
//		ASSERT(packet_buffer_find_packet(&s.buf, &p) == bp);
//
//		ASSERT(packet_buffer_get_packet_for_sending(&s.buf) != bp);
//
//		{
//			struct packet p1 = { {1, {{2, 3}}, {{4, 5}}, 6} };
//			struct buffered_packet *bpp = packet_buffer_get_packet_for_sending(&s.buf);
//			ASSERT(packet_buffer_find_packet(&s.buf, &p1) == bpp);
//			ASSERT(memcmp("joha", packet_buffer_get_data(bpp), packet_buffer_data_len(bpp)) == 0);
//			packet_buffer_free(&s.buf, bpp);
//			ASSERT(packet_buffer_find_packet(&s.buf, &p1) == NULL);
//		}
//
//		ASSERT(packet_buffer_get_packet_for_sending(&s.buf) == bp);;
//	}
	ASSERT(s.guard1 == 'A');
	ASSERT(s.guard2 == 'B');
	ASSERT(s.guard3 == 'C');
	ASSERT(s.guard4 == 'D');

	return 0;
}
