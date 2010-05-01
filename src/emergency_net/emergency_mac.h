#ifndef _EMERGENCY_MAC_H_
#define _EMERGENCY_MAC_H_

#include "net/mac/mac.h"
#include "dev/radio.h"

extern const struct mac_driver emergency_mac_driver;

const struct mac_driver* emergency_mac_init(const struct radio_driver *r);

#endif
