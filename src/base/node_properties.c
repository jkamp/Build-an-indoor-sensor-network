#include "base/node_properties.h"

#include "contiki-conf.h"

#include "dev/xmem.h"

#include "base/log.h"

int node_properties_restore(void *out, int size) {
	unsigned char tag[2];
	xmem_pread(tag, 2, 0);
	if(tag[0] == 0xde && tag[1] == 0xad) {
		xmem_pread(out, size, 2);
		return 1;
	} 

	return 0;
}

void node_properties_burn(const void *in, int size) {
	TRACE("E2LLO\n");
	unsigned char tag[2];
	TRACE("EHLLO\n");
	tag[0] = 0xde;
	tag[1] = 0xad;
	TRACE("erasing\n");
	xmem_erase(XMEM_ERASE_UNIT_SIZE, 0);
	TRACE("erasing ok\n");

	ASSERT(size < XMEM_ERASE_UNIT_SIZE);
	xmem_pwrite(tag, 2, 0);
	TRACE("writing1 ok\n");
	xmem_pwrite(in, size, 2);
	TRACE("writing2 ok\n");
}
/*---------------------------------------------------------------------------*/
