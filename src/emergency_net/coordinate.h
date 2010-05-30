#ifndef _COORDINATE_H_
#define _COORDINATE_H_

#include "contiki-conf.h"

struct coordinate {
	uint8_t x[2];
	uint8_t y[2];
};

uint16_t coordinate_distance(const struct coordinate *from, 
		const struct coordinate *to);

void coordinate_set_node_coord(const struct coordinate *coord);

int coordinate_cmp(const void *lhs, 
		const void *rhs);

void coordinate_copy(struct coordinate *to, 
		const struct coordinate *from);


extern struct coordinate coordinate_node;
extern const struct coordinate coordinate_null;

#endif
