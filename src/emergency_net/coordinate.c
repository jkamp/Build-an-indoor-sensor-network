#include "coordinate.h"

struct coordinate coordinate_node;
const struct coordinate coordinate_null = {0, 0};

static inline
uint16_t coord_abs(int32_t a) {
	return a < 0 ? -a : a;
}

uint16_t coordinate_distance(const struct coordinate *from, const struct coordinate *to) {
	return coord_abs(to->x - from->x) + coord_abs(to->y - from->y);
}

void
coordinate_set_node_coord(const struct coordinate *coord) {
	coordinate_node = *coord;
}
