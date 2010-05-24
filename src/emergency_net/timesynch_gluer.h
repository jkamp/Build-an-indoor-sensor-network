#ifndef _TIMESYNCH_GLUER_H_
#define _TIMESYNCH_GLUER_H_

#include "net/rime/rimeaddr.h"

#include "contiki-conf.h"

#define AUTHORITY_LEVEL_MAX 0xFF

typedef uint8_t authority_t;

void set_authority_level(authority_t al);
void set_authority_seqno(uint8_t as);
void set_timesynch_channel(uint8_t ch);

static
authority_t timesynch_rimeaddr_to_authority(const rimeaddr_t *addr);

extern authority_t authority_level;
extern uint8_t authority_seqno;
extern uint8_t timesynch_channel;


static inline
authority_t timesynch_rimeaddr_to_authority(const rimeaddr_t *addr) {
	return addr->u8[0];
}
#endif
