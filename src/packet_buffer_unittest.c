
#include "string.h"

#include "net/packet_buffer.h"

#include "base/log.h"


struct test_s {
	char guard1;
	char guard2;
	PACKET_BUFFER(buf, 10);
	char guard3;
	char guard4;
};

int main(void) {
	struct test_s s;
	s.guard1 = 'a';
	s.guard2 = 'b';
	s.guard3 = 'c';
	s.guard4 = 'd';
	PACKET_BUFFER_INIT_WITH_STRUCT(&s, buf, 10);

	ASSERT(packet_buffer_has_packet_for_sending(&s.buf) == 0);
	ASSERT(packet_buffer_get_packet_for_sending(&s.buf) == NULL);

	{
		struct packet p = { {1, {{2, 3}}, {{4, 5}}, 6} };
		struct buffered_packet *bp = packet_buffer_packet(&s.buf, &p, "joha", 4, 1);
		struct packet *pp = packet_buffer_get_packet(bp);

		ASSERT(memcmp(pp, &p, sizeof(struct packet)) == 0);
		ASSERT(packet_buffer_data_len(bp) == 4);
		ASSERT(packet_buffer_times_acked(bp) == 0);
		ASSERT(packet_buffer_times_sent(bp) == 0);
		ASSERT(memcmp("joha", packet_buffer_get_data(bp), packet_buffer_data_len(bp)) == 0);
		ASSERT(packet_buffer_find_packet(&s.buf, &p) == bp);

		ASSERT(packet_buffer_get_packet_for_sending(&s.buf) == bp);
	}
	{
		struct packet p = { {3, {{4, 5}}, {{6,7}},8} };
		struct buffered_packet *bp = packet_buffer_packet(&s.buf, &p, "joH", 3, 2);
		struct packet *pp = packet_buffer_get_packet(bp);

		ASSERT(memcmp(pp, &p, sizeof(struct packet)) == 0);
		ASSERT(packet_buffer_data_len(bp) == 3);
		ASSERT(packet_buffer_times_acked(bp) == 0);
		ASSERT(packet_buffer_times_sent(bp) == 0);
		ASSERT(memcmp("joH", packet_buffer_get_data(bp), packet_buffer_data_len(bp)) == 0);
		ASSERT(packet_buffer_find_packet(&s.buf, &p) == bp);

		ASSERT(packet_buffer_get_packet_for_sending(&s.buf) != bp);

		{
			struct packet p1 = { {1, {{2, 3}}, {{4, 5}}, 6} };
			struct buffered_packet *bpp = packet_buffer_get_packet_for_sending(&s.buf);
			ASSERT(packet_buffer_find_packet(&s.buf, &p1) == bpp);
			ASSERT(memcmp("joha", packet_buffer_get_data(bpp), packet_buffer_data_len(bpp)) == 0);
			packet_buffer_free(&s.buf, bpp);
			ASSERT(packet_buffer_find_packet(&s.buf, &p1) == NULL);
		}

		ASSERT(packet_buffer_get_packet_for_sending(&s.buf) == bp);;
	}
	ASSERT(s.guard1 == 'a');
	ASSERT(s.guard2 == 'b');
	ASSERT(s.guard3 == 'c');
	ASSERT(s.guard4 == 'd');

	return 0;
}
