#include "base/queue_buffer.h"


#include "emergency_net/neighbor_node.h"

#include "base/log.h"

#define ITEM(qb, item_nr) ((struct queue_buffer_s*) \
		(((char*)(qb)->buffer_begin) + \
		 (item_nr)*((qb)->buffer_size+QUEUE_BUFFER_S_HDR_SIZE)))

static void add_tail(struct queue_buffer_s **head, struct queue_buffer_s *b){
	b->next = NULL;

	if (*head != NULL) {
		struct queue_buffer_s *iter = *head;
		while (iter->next != NULL)
			iter = iter->next;
		iter->next = b;
	} else {
		*head = b;
	}
}

inline
static void add_head(struct queue_buffer_s **head, struct queue_buffer_s *new_head) {
	new_head->next = *head;
	*head = new_head;
}

static struct queue_buffer_s* allocate_buffer_space_back(struct queue_buffer *qb) {
	if (qb->unused_head != NULL) {
		struct queue_buffer_s *buf = qb->unused_head;
		qb->unused_head = qb->unused_head->next;
		add_tail(&qb->used_head, buf);
		return buf;
	}

	return NULL;
}

static struct queue_buffer_s* allocate_buffer_space_front(struct queue_buffer *qb) {
	if (qb->unused_head != NULL) {
		struct queue_buffer_s *buf = qb->unused_head;
		qb->unused_head = qb->unused_head->next;
		add_head(&qb->used_head, buf);
		return buf;
	}

	return NULL;
}


void queue_buffer_init(struct queue_buffer *qb, uint8_t buffer_size, uint8_t
		num_items, void *buffer_begin) {

	int i;
	struct queue_buffer_s *b;

	qb->queue_size = 0;
	qb->queue_max_size = num_items;
	qb->buffer_size = buffer_size;
	qb->buffer_begin = buffer_begin;

	qb->unused_head = NULL;
	for(i = 0; i < qb->queue_max_size; ++i) {
		b = ITEM(qb, i);
		add_tail(&qb->unused_head, b);
	}
	qb->used_head = NULL;
	qb->iterator = NULL;
}

void* queue_buffer_alloc_back(struct queue_buffer* qb) {
	struct queue_buffer_s *b = allocate_buffer_space_back(qb);
	if (b != NULL) {
		++qb->queue_size;
		return b->data;
	}

	return NULL;
}

void* queue_buffer_alloc_front(struct queue_buffer* qb) {
	struct queue_buffer_s *b = allocate_buffer_space_front(qb);
	if (b != NULL) {
		++qb->queue_size;
		return b->data;
	}

	return NULL;
}

void* queue_buffer_push_front(struct queue_buffer *qb, const void *item) {

	struct queue_buffer_s *b = allocate_buffer_space_front(qb);
	//LOG("push front: %x\n", b);
	//TRACE("metric: %u\n", ((struct neighbor_node*)item)->bp.metric);
	if (b != NULL) {
		++qb->queue_size;
		memcpy(b->data, item, qb->buffer_size);
		ASSERT(memcmp(item, b->data, qb->buffer_size) == 0);
		//TRACE("metric: %u\n", ((struct neighbor_node*)b->data)->bp.metric);
		return b->data;
	}

	return NULL;
}

void* queue_buffer_push_back(struct queue_buffer* qb, const void *item) {

	struct queue_buffer_s *b = allocate_buffer_space_back(qb);
	if (b != NULL) {
		++qb->queue_size;
		memcpy(b->data, item, qb->buffer_size);
		return b->data;
	}

	return NULL;
}

void queue_buffer_pop_front(struct queue_buffer* qb, void* item_out) {
	struct queue_buffer_s *i = qb->used_head;
	ASSERT(i != NULL);

	if (item_out != NULL) {
		memcpy(item_out, i->data, qb->buffer_size);
	}

	qb->used_head = i->next;
	add_head(&qb->unused_head, i);
	--qb->queue_size;
}

void queue_buffer_pop_back(struct queue_buffer* qb, void* item_out) {
	struct queue_buffer_s *i = qb->used_head;
	struct queue_buffer_s *prev = NULL;

	ASSERT(i != NULL);

	while (i->next != NULL) {
		prev = i;
		i = i->next;
	}

	if (item_out != NULL) {
		memcpy(item_out, i->data, qb->buffer_size);
	}

	add_head(&qb->unused_head, i);
	if (prev != NULL) {
		prev->next = NULL;
	} else {
		qb->used_head = NULL;
	}
	--qb->queue_size;
}


void queue_buffer_free(struct queue_buffer* qb, void* item) {
	struct queue_buffer_s *prev = NULL;
	struct queue_buffer_s *i = qb->used_head;
	
	for(; i != NULL; i = i->next) {
		if ( i->data == item ){
			if (prev == NULL) {
				qb->used_head = qb->used_head->next;
			} else {
				prev->next = i->next;
			}
			add_head(&qb->unused_head, i);
			--qb->queue_size;
			return;
		} 
		prev = i;
	}

	/* should never happen */
	ASSERT(0);
}

void* queue_buffer_find(struct queue_buffer* qb, const void *item, 
		int (*comparer)(const void*, const void*)) {

	struct queue_buffer_s *i = qb->used_head;
	for(; i != NULL; i = i->next) {
		if (comparer(i->data, item)) {
			return i->data;
		}
	}

	return NULL;
}

void queue_buffer_clear(struct queue_buffer *qb) {
	qb->queue_size = 0;
	if (qb->used_head != NULL) {
		struct queue_buffer_s *s = qb->unused_head;
		if(s != NULL) {
			for (; s->next != NULL; s = s->next);

			s->next = qb->used_head;
		} else {
			qb->unused_head = qb->used_head;
		}

		qb->used_head = NULL;
	}
	qb->iterator = NULL;
}
