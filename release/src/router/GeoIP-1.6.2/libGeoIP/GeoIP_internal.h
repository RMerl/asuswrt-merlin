#ifndef GEOIP_INTERNAL_H
#define GEOIP_INTERNAL_H

#include "GeoIP.h"

GEOIP_API unsigned int _GeoIP_seek_record_gl(GeoIP *gi, unsigned long ipnum,
                                             GeoIPLookup * gl);
GEOIP_API unsigned int _GeoIP_seek_record_v6_gl(GeoIP *gi, geoipv6_t ipnum,
                                                GeoIPLookup * gl);
GEOIP_API geoipv6_t _GeoIP_addr_to_num_v6(const char *addr);

GEOIP_API unsigned long _GeoIP_lookupaddress(const char *host);
GEOIP_API geoipv6_t _GeoIP_lookupaddress_v6(const char *host);
GEOIP_API int __GEOIP_V6_IS_NULL(geoipv6_t v6);

GEOIP_API void _GeoIP_setup_dbfilename();
GEOIP_API char *_GeoIP_full_path_to(const char *file_name);

/* deprecated */
GEOIP_API unsigned int _GeoIP_seek_record(GeoIP *gi, unsigned long ipnum);
GEOIP_API unsigned int _GeoIP_seek_record_v6(GeoIP *gi, geoipv6_t ipnum);

#endif
