#ifndef _QUEUE_BUFFER_H_
#define _QUEUE_BUFFER_H_

#include "stdint.h"
#include "stddef.h" /* NULL */

#define QUEUE_BUFFER_S_HDR_SIZE (sizeof(struct queue_buffer_s)-sizeof(uint8_t))

#define QUEUE_BUFFER(name, buffersize, num_items) \
	uint8_t name##_buffer[num_items][QUEUE_BUFFER_S_HDR_SIZE+buffersize]; \
	struct queue_buffer name

#define QUEUE_BUFFER_INIT_WITH_STRUCT(s, name, buffersize, num_items) \
	queue_buffer_init(&(s)->name, buffersize, num_items, (s)->name##_buffer)

struct queue_buffer_s {
	struct queue_buffer_s *next;
	uint8_t data[1];
};

struct queue_buffer {
	uint8_t queue_size;
	uint8_t queue_max_size;
	uint8_t buffer_size;
	void *buffer_begin;
	struct queue_buffer_s *unused_head;
	struct queue_buffer_s *used_head;
	struct queue_buffer_s *iterator;
};


void queue_buffer_init(struct queue_buffer *qb, uint8_t buffer_size, uint8_t
		num_items, void *buffer_begin);

/** (alloc_front is faster) */
void* queue_buffer_alloc_front(struct queue_buffer *qb);
void* queue_buffer_alloc_back(struct queue_buffer *qb);

/** (push_front is faster) */
void* queue_buffer_push_front(struct queue_buffer *qb, const void *item);
void* queue_buffer_push_back(struct queue_buffer *qb, const void *item);

/** 
 * Pops from the ends. The item popped gets copied to item_out. item_out is
 * optional and can be NULL.
 *
 * Dont call these functions unless you have something to pop (e.g. dont call
 * if queue buffer is empty). Null pointer dereference will burn your ass then.
 *
 * (pop_front is faster)
 */
void queue_buffer_pop_front(struct queue_buffer* qb, void *item_out);
void queue_buffer_pop_back(struct queue_buffer* qb, void *item_out);


static inline void* 
queue_buffer_begin(struct queue_buffer *qb);
static inline void* 
queue_buffer_next(struct queue_buffer *qb);

/* 
 * Finds if item is in queue_buffer. Comparerer should return 1 if items are
 * equal. 0 otherwise. 
 * 
 * Function returns queued item if found, NULL otherwise.  
 */
void* queue_buffer_find(struct queue_buffer* qb, const void *item, 
		int (*comparer)(const void *queued_item, const void *supplied_item));

static inline int 
queue_buffer_size(const struct queue_buffer *qb);

static inline int 
queue_buffer_max_size(const struct queue_buffer *qb);

void queue_buffer_free(struct queue_buffer *qb, void* item);


/************************* Inline Definitions **************************/

void* queue_buffer_begin(struct queue_buffer *qb) {
	qb->iterator = qb->used_head;
	if (qb->iterator != NULL)
		return qb->iterator->data;

	return NULL;
}

void* queue_buffer_next(struct queue_buffer *qb) {
	qb->iterator = qb->iterator->next;
	if (qb->iterator != NULL)
		return qb->iterator->data;

	return NULL;
}

int queue_buffer_size(const struct queue_buffer *qb) {
	return qb->queue_size;
}

int queue_buffer_max_size(const struct queue_buffer *qb) {
	return qb->queue_max_size;
}
#endif
