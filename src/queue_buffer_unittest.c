#include "contiki.h"

#include "dev/serial-line.h"

#include "string.h"

//#include "net/packet.h"

#include "base/queue_buffer.h"

#include "base/log.h"

struct t {
	char a;
	char b;
	char c;
};

int cmparer(const void *l, const void *r) {
	const struct t *ll = (struct t*)l;
	const struct t *rr = (struct t*)r;

	return ll->a == rr->a &&
		ll->b == rr->b &&
		ll->c == rr->c;
}

struct test_s {
	char guard1;
	char guard2;
	QUEUE_BUFFER(qb, sizeof(struct t), 8);
	char guard3;
	char guard4;
};

PROCESS(test_process, "testqueue");
AUTOSTART_PROCESSES(&test_process);

PROCESS_THREAD(test_process, ev, data) {

	PROCESS_BEGIN();

	while(1) {
		PROCESS_WAIT_EVENT();
		if (ev == serial_line_event_message && data != NULL) {
			struct test_s s;
			s.guard1 = 'A';
			s.guard2 = 'B';
			s.guard3 = 'C';
			s.guard4 = 'D';
			LOG("BEGINING TEST\n");

			QUEUE_BUFFER_INIT_WITH_STRUCT(&s, qb, sizeof(struct t), 8);

			ASSERT(queue_buffer_size(&s.qb) == 0);
			ASSERT(queue_buffer_max_size(&s.qb) == 8);
			ASSERT(queue_buffer_begin(&s.qb) == NULL);

			{
				struct t tt1 = {'a','b','c'};
				struct t *qt1 = (struct t*)queue_buffer_push_front(&s.qb, &tt1);
				ASSERT(memcmp(&tt1, qt1, sizeof(struct t)) == 0);

				ASSERT(queue_buffer_begin(&s.qb) == qt1);
				ASSERT(queue_buffer_next(&s.qb) == NULL);
				ASSERT(queue_buffer_size(&s.qb) == 1);
				ASSERT(queue_buffer_max_size(&s.qb) == 8);
				ASSERT(queue_buffer_find(&s.qb, &tt1, cmparer) == qt1);

				struct t tt2 = {'d','e','f'};
				struct t *qt2 = (struct t*)queue_buffer_push_front(&s.qb, &tt2);
				ASSERT(memcmp(&tt2, qt2, sizeof(struct t)) == 0);

				ASSERT(queue_buffer_begin(&s.qb) == qt2);
				ASSERT(queue_buffer_next(&s.qb) == qt1);
				ASSERT(queue_buffer_next(&s.qb) == NULL);
				ASSERT(queue_buffer_size(&s.qb) == 2);
				ASSERT(queue_buffer_max_size(&s.qb) == 8);
				ASSERT(queue_buffer_find(&s.qb, &tt1, cmparer) == qt1);
				ASSERT(queue_buffer_find(&s.qb, &tt2, cmparer) == qt2);

#if 0
				queue_buffer_free(&s.qb, qt2);
				ASSERT(queue_buffer_begin(&s.qb) == qt1);
				ASSERT(queue_buffer_next(&s.qb) == NULL);
				ASSERT(queue_buffer_size(&s.qb) == 1);
				ASSERT(queue_buffer_max_size(&s.qb) == 8);
				ASSERT(queue_buffer_find(&s.qb, &tt1, cmparer) == qt1);
				ASSERT(queue_buffer_find(&s.qb, &tt2, cmparer) == NULL);
#else
				queue_buffer_free(&s.qb, qt1);
				ASSERT(queue_buffer_begin(&s.qb) == qt2);
				ASSERT(queue_buffer_next(&s.qb) == NULL);
				ASSERT(queue_buffer_size(&s.qb) == 1);
				ASSERT(queue_buffer_max_size(&s.qb) == 8);
				ASSERT(queue_buffer_find(&s.qb, &tt1, cmparer) == NULL);
				ASSERT(queue_buffer_find(&s.qb, &tt2, cmparer) == qt2);

#endif
			}

			ASSERT(s.guard1 == 'A');
			ASSERT(s.guard2 == 'B');
			ASSERT(s.guard3 == 'C');
			ASSERT(s.guard4 == 'D');
			LOG("TEST OK\n");
		}
	}

	PROCESS_END();
}
