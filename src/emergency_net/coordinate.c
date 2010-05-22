#include "coordinate.h"
#include "base/log.h"

struct coordinate coordinate_node;
const struct coordinate coordinate_null = {0, 0};

static inline
uint16_t coord_abs(int32_t a) {
	return a < 0 ? -a : a;
}

uint16_t coordinate_distance(const struct coordinate *from, const struct
		coordinate *to) {
	uint16_t diffx = coord_abs((int32_t)(to->x) - (int32_t)(from->x));
	uint16_t diffy = coord_abs((int32_t)(to->y) - (int32_t)(from->y));
	return diffx + diffy;
}

int coordinate_cmp(const struct coordinate *lhs, 
		const struct coordinate *rhs) {
	return lhs->x == rhs->x && lhs->y == rhs->y;
}

void
coordinate_set_node_coord(const struct coordinate *coord) {
	coordinate_node = *coord;
}
