#ifndef _UTIL_H_
#define _UTIL_H_

static inline
void uint16_to_uint8(const uint16_t in, uint8_t out[2]) {
	out[0] = (uint8_t)((in&0xFF00)>>8);
	out[1] = (uint8_t)(in&0xFF);
}

static inline
void uint8_to_uint16(const uint8_t in[2],
	   	uint16_t *out) {
	*out = (uint16_t)((in[0]<<8) | in[1]);
}
#endif
