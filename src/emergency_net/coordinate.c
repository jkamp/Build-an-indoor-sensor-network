#include "coordinate.h"
#include "base/log.h"

#include "base/util.h"

struct coordinate coordinate_node;
const struct coordinate coordinate_null = {{0,0}, {0,0}};

static inline
uint16_t coord_abs(int32_t a) {
	return a < 0 ? -a : a;
}

uint16_t coordinate_distance(const struct coordinate *from, const struct
		coordinate *to) {
	uint16_t from16;
	uint16_t to16;
	uint16_t diffx;
	uint16_t diffy;
    uint8_to_uint16(from->x, &from16);
    uint8_to_uint16(to->x, &to16);
	diffx = coord_abs((int32_t)(to16) - (int32_t)(from16));

    uint8_to_uint16(from->y, &from16);
    uint8_to_uint16(to->y, &to16);
	diffy = coord_abs((int32_t)(to16) - (int32_t)(from16));

	return diffx + diffy;
}

int coordinate_cmp(const void *l, 
		const void *r) {

	const struct coordinate *lhs = (struct coordinate*)l;
	const struct coordinate *rhs = (struct coordinate*)r;

	return lhs->x[0] == rhs->x[0] && 
		lhs->x[1] == rhs->x[1] && 
		lhs->y[0] == rhs->y[0] && 
		lhs->y[1] == rhs->y[1];
}

void
coordinate_set_node_coord(const struct coordinate *coord) {
//	LOG("Setting coordinate: (%d.%d, %d.%d)", 
//			coord->x[0],
//			coord->x[1],
//			coord->y[0],
//			coord->y[1]);
	memcpy(&coordinate_node, coord, sizeof(struct coordinate));
}

void coordinate_copy(struct coordinate *to, 
		const struct coordinate *from) {
	memcpy(to, from, sizeof(struct coordinate));
}
