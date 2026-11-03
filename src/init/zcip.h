#ifndef ANT_ZCIP_H
#define ANT_ZCIP_H
#include <stdint.h>
/*! RFC 3927 link-local on ifname.
 *  prefer_hbo: host-order 169.254.x.y or 0 = random.
 *  \return host-order IPv4 or 0 on failure.
 */
uint32_t zcip_run(const char *ifname, uint32_t prefer_hbo);
#endif
