#include "emergency_net/timesynch_gluer.h"

authority_t authority_level;
uint8_t authority_seqno;

void set_authority_level(authority_t al) {
	authority_level = al;
}
void set_authority_seqno(uint8_t as) {
	authority_seqno = as;
}
