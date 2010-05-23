#include "base/node_properties.h"

#include "contiki-conf.h"

#include "dev/xmem.h"

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
	unsigned char tag[2];
	tag[0] = 0xde;
	tag[1] = 0xad;
	xmem_erase(XMEM_ERASE_UNIT_SIZE, 0);
	xmem_pwrite(tag, 2, 0);
	xmem_pwrite(in, size, 2);
}
/*---------------------------------------------------------------------------*/
