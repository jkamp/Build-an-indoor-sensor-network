#ifndef _COORDINATE_H_
#define _COORDINATE_H_

#include "contiki-conf.h"

struct coordinate {
	uint16_t x;
	uint16_t y;
};

uint16_t coordinate_distance(const struct coordinate *from, 
		const struct coordinate *to);

void coordinate_set_node_coord(const struct coordinate *coord);

int coordinate_cmp(const struct coordinate *lhs, 
		const struct coordinate *rhs);

extern struct coordinate coordinate_node;
extern const struct coordinate coordinate_null;

#endif
