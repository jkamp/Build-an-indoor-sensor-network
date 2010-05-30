#ifndef _NEIGHBOR_NODE_H_
#define _NEIGHBOR_NODE_H_

#include "net/rime/rimeaddr.h"
#include "emergency_net/coordinate.h"

#include "base/util.h"

typedef uint16_t metric_t;
#define METRIC_T_MAX 0xFFFF

typedef uint16_t distance_t;
#define DISTANCE_T_MAX 0xFFFF

struct neighbor_node_best_path {
	uint8_t metric[2]; /* to nearest exit from neighbor */
	rimeaddr_t points_to;
	uint8_t hops; /* to nearest exit from neighbor */
};

struct neighbor_node {
	rimeaddr_t addr;
	struct coordinate coord;
	uint8_t distance[2]; /* to neighbor. So we dont have to calculate it every time */
	struct neighbor_node_best_path bp;
	uint8_t warnings; /* issued when the neighbor is not responding */
};

extern const struct neighbor_node_best_path neighbor_node_best_path_max;

static
const rimeaddr_t* neighbor_node_addr(const struct neighbor_node *nn);

static
void neighbor_node_set_addr(struct neighbor_node *nn, const rimeaddr_t *addr);

static
void neighbor_node_set_coordinate(struct neighbor_node *nn, 
		const struct coordinate *coord);

static
const struct coordinate* neighbor_node_coord(const struct neighbor_node *nn);

static
metric_t neighbor_node_metric(const struct neighbor_node *nn);

static
int neighbor_node_points_to_us(const struct neighbor_node *nn);

static
uint8_t neighbor_node_hops(const struct neighbor_node *nn);

static
distance_t neighbor_node_distance(const struct neighbor_node *nn);

static
void neighbor_node_set_best_path(struct neighbor_node *nn, 
		const struct neighbor_node_best_path *bp);


/* inline definitions */

static inline
const rimeaddr_t* neighbor_node_addr(const struct neighbor_node *nn) {
	return &nn->addr;
}


static inline
void neighbor_node_set_addr(struct neighbor_node *nn, const rimeaddr_t *addr) {
	rimeaddr_copy(&nn->addr, addr);
}

static inline
void neighbor_node_set_coordinate(struct neighbor_node *nn, 
		const struct coordinate *coord) {
	coordinate_copy(&nn->coord, coord);
	uint16_to_uint8(coordinate_distance(coord, &coordinate_node), nn->distance);
}

static inline
void neighbor_node_set_best_path(struct neighbor_node *nn, 
		const struct neighbor_node_best_path *bp) {
	memcpy(&nn->bp, bp, sizeof(struct neighbor_node_best_path));
	rimeaddr_copy(&nn->bp.points_to, &bp->points_to);
	nn->bp.hops = bp->hops;
}

static inline
const struct coordinate* neighbor_node_coord(const struct neighbor_node *nn) {
	return &nn->coord;
}

static inline
metric_t neighbor_node_metric(const struct neighbor_node *nn) {
	uint16_t metric16;
	uint8_to_uint16(nn->bp.metric, &metric16);
	return metric16;
}

static inline
int neighbor_node_points_to_us(const struct neighbor_node *nn) {
	return rimeaddr_cmp(&nn->bp.points_to, &rimeaddr_node_addr);
}

static inline
uint8_t neighbor_node_hops(const struct neighbor_node *nn) {
	return nn->bp.hops;
}

static inline
distance_t neighbor_node_distance(const struct neighbor_node *nn) {
	uint16_t dist16;
	uint8_to_uint16(nn->distance, &dist16);
	return dist16;
}
#endif
