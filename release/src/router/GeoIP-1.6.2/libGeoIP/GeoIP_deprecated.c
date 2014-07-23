
#include "GeoIP_internal.h"

char *
GeoIP_org_by_ipnum(GeoIP * gi, unsigned long ipnum)
{
    GeoIPLookup gl;
    return GeoIP_name_by_ipnum_gl(gi, ipnum, &gl);
}

char *
GeoIP_org_by_ipnum_v6(GeoIP * gi, geoipv6_t ipnum)
{
    GeoIPLookup gl;
    return GeoIP_name_by_ipnum_v6_gl(gi, ipnum, &gl);
}

char *
GeoIP_org_by_addr(GeoIP * gi, const char *addr)
{
    GeoIPLookup gl;
    return GeoIP_name_by_addr_gl(gi, addr, &gl);
}

char *
GeoIP_org_by_addr_v6(GeoIP * gi, const char *addr)
{
    GeoIPLookup gl;
    return GeoIP_name_by_addr_v6_gl(gi, addr, &gl);
}

char *
GeoIP_org_by_name(GeoIP * gi, const char *name)
{
    GeoIPLookup gl;
    return GeoIP_name_by_name_gl(gi, name, &gl);
}

char *
GeoIP_org_by_name_v6(GeoIP * gi, const char *name)
{
    GeoIPLookup gl;
    return GeoIP_name_by_name_v6_gl(gi, name, &gl);
}

int
GeoIP_last_netmask(GeoIP * gi)
{
    return gi->netmask;
}

unsigned int
_GeoIP_seek_record_v6(GeoIP * gi, geoipv6_t ipnum)
{
    GeoIPLookup gl;
    return _GeoIP_seek_record_v6_gl(gi, ipnum, &gl);
}

unsigned int
_GeoIP_seek_record(GeoIP * gi, unsigned long ipnum)
{
    GeoIPLookup gl;
    return _GeoIP_seek_record_gl(gi, ipnum, &gl);
}

const char *
GeoIP_country_code_by_name_v6(GeoIP * gi, const char *name)
{
    GeoIPLookup gl;
    return GeoIP_country_code_by_name_v6_gl(gi, name, &gl);
}

const char *
GeoIP_country_code_by_name(GeoIP * gi, const char *name)
{
    GeoIPLookup gl;
    return GeoIP_country_code_by_name_gl(gi, name, &gl);
}

const char *
GeoIP_country_code3_by_name_v6(GeoIP * gi, const char *name)
{
    GeoIPLookup gl;
    return GeoIP_country_code3_by_name_v6_gl(gi, name, &gl);
}

const char *
GeoIP_country_code3_by_name(GeoIP * gi, const char *name)
{
    GeoIPLookup gl;
    return GeoIP_country_code3_by_name_gl(gi, name, &gl);
}

const char *
GeoIP_country_name_by_name_v6(GeoIP * gi, const char *name)
{
    GeoIPLookup gl;
    return GeoIP_country_name_by_name_v6_gl(gi, name, &gl);
}

const char *
GeoIP_country_name_by_name(GeoIP * gi, const char *name)
{
    GeoIPLookup gl;
    return GeoIP_country_name_by_name_gl(gi, name, &gl);
}

int
GeoIP_id_by_name(GeoIP * gi, const char *name)
{
    GeoIPLookup gl;
    return GeoIP_id_by_name_gl(gi, name, &gl);
}
int
GeoIP_id_by_name_v6(GeoIP * gi, const char *name)
{
    GeoIPLookup gl;
    return GeoIP_id_by_name_v6_gl(gi, name, &gl);
}

const char *
GeoIP_country_code_by_addr_v6(GeoIP * gi, const char *addr)
{
    GeoIPLookup gl;
    return GeoIP_country_code_by_addr_v6_gl(gi, addr, &gl);
}
const char *
GeoIP_country_code_by_addr(GeoIP * gi, const char *addr)
{
    GeoIPLookup gl;
    return GeoIP_country_code_by_addr_gl(gi, addr, &gl);
}

const char *
GeoIP_country_code3_by_addr_v6(GeoIP * gi, const char *addr)
{
    GeoIPLookup gl;
    return GeoIP_country_code3_by_addr_v6_gl(gi, addr, &gl);
}

const char *
GeoIP_country_code3_by_addr(GeoIP * gi, const char *addr)
{
    GeoIPLookup gl;
    return GeoIP_country_code3_by_addr_gl(gi, addr, &gl);
}

const char *
GeoIP_country_name_by_addr_v6(GeoIP * gi, const char *addr)
{
    GeoIPLookup gl;
    return GeoIP_country_name_by_addr_v6_gl(gi, addr, &gl);
}
const char *
GeoIP_country_name_by_addr(GeoIP * gi, const char *addr)
{
    GeoIPLookup gl;
    return GeoIP_country_name_by_addr_gl(gi, addr, &gl);
}

const char *
GeoIP_country_name_by_ipnum(GeoIP * gi, unsigned long ipnum)
{
    GeoIPLookup gl;
    return GeoIP_country_name_by_ipnum_gl(gi, ipnum, &gl);
}

const char *
GeoIP_country_name_by_ipnum_v6(GeoIP * gi, geoipv6_t ipnum)
{
    GeoIPLookup gl;
    return GeoIP_country_name_by_ipnum_v6_gl(gi, ipnum, &gl);
}

const char *
GeoIP_country_code_by_ipnum_v6(GeoIP * gi, geoipv6_t ipnum)
{
    GeoIPLookup gl;
    return GeoIP_country_code_by_ipnum_v6_gl(gi, ipnum, &gl);
}


const char *
GeoIP_country_code_by_ipnum(GeoIP * gi, unsigned long ipnum)
{
    GeoIPLookup gl;
    return GeoIP_country_code_by_ipnum_gl(gi, ipnum, &gl);
}

const char *
GeoIP_country_code3_by_ipnum(GeoIP * gi, unsigned long ipnum)
{
    GeoIPLookup gl;
    return GeoIP_country_code3_by_ipnum_gl(gi, ipnum, &gl);
}

const char *
GeoIP_country_code3_by_ipnum_v6(GeoIP * gi, geoipv6_t ipnum)
{
    GeoIPLookup gl;
    return GeoIP_country_code3_by_ipnum_v6_gl(gi, ipnum, &gl);
}

int
GeoIP_country_id_by_addr_v6(GeoIP * gi, const char *addr)
{
    GeoIPLookup gl;
    return GeoIP_id_by_addr_v6_gl(gi, addr, &gl);
}

int
GeoIP_country_id_by_addr(GeoIP * gi, const char *addr)
{
    GeoIPLookup gl;
    return GeoIP_id_by_addr_gl(gi, addr, &gl);
}
int
GeoIP_country_id_by_name_v6(GeoIP * gi, const char *host)
{
    GeoIPLookup gl;
    return GeoIP_id_by_name_v6_gl(gi, host, &gl);
}
int
GeoIP_country_id_by_name(GeoIP * gi, const char *host)
{
    GeoIPLookup gl;
    return GeoIP_id_by_name_gl(gi, host, &gl);
}
int
GeoIP_id_by_addr_v6(GeoIP * gi, const char *addr)
{
    GeoIPLookup gl;
    return GeoIP_id_by_addr_v6_gl(gi, addr, &gl);
}
int
GeoIP_id_by_addr(GeoIP * gi, const char *addr)
{
    GeoIPLookup gl;
    return GeoIP_id_by_addr_gl(gi, addr, &gl);
}

int
GeoIP_id_by_ipnum_v6(GeoIP * gi, geoipv6_t ipnum)
{
    GeoIPLookup gl;
    return GeoIP_id_by_ipnum_v6_gl(gi, ipnum, &gl);
}
int
GeoIP_id_by_ipnum(GeoIP * gi, unsigned long ipnum)
{
    GeoIPLookup gl;
    return GeoIP_id_by_ipnum_gl(gi, ipnum, &gl);
}
void
GeoIP_assign_region_by_inetaddr(GeoIP * gi, unsigned long inetaddr,
                                GeoIPRegion * region)
{
    GeoIPLookup gl;
    GeoIP_assign_region_by_inetaddr_gl(gi, inetaddr, region, &gl);
}

void
GeoIP_assign_region_by_inetaddr_v6(GeoIP * gi, geoipv6_t inetaddr,
                                   GeoIPRegion * region)
{
    GeoIPLookup gl;
    GeoIP_assign_region_by_inetaddr_v6_gl(gi, inetaddr, region, &gl);
}


GeoIPRegion *
GeoIP_region_by_addr(GeoIP * gi, const char *addr)
{
    GeoIPLookup gl;
    return GeoIP_region_by_addr_gl(gi, addr, &gl);
}
GeoIPRegion *
GeoIP_region_by_addr_v6(GeoIP * gi, const char *addr)
{
    GeoIPLookup gl;
    return GeoIP_region_by_addr_v6_gl(gi, addr, &gl);
}

GeoIPRegion *
GeoIP_region_by_name(GeoIP * gi, const char *name)
{
    GeoIPLookup gl;
    return GeoIP_region_by_name_gl(gi, name, &gl);
}
GeoIPRegion *
GeoIP_region_by_name_v6(GeoIP * gi, const char *name)
{
    GeoIPLookup gl;
    return GeoIP_region_by_name_v6_gl(gi, name, &gl);
}

GeoIPRegion *
GeoIP_region_by_ipnum(GeoIP * gi, unsigned long ipnum)
{
    GeoIPLookup gl;
    return GeoIP_region_by_ipnum_gl(gi, ipnum, &gl);
}
GeoIPRegion *
GeoIP_region_by_ipnum_v6(GeoIP * gi, geoipv6_t ipnum)
{
    GeoIPLookup gl;
    return GeoIP_region_by_ipnum_v6_gl(gi, ipnum, &gl);
}

char **
GeoIP_range_by_ip(GeoIP * gi, const char *addr)
{
    GeoIPLookup gl;
    return GeoIP_range_by_ip_gl(gi, addr, &gl);
}

char *
GeoIP_name_by_ipnum(GeoIP * gi, unsigned long ipnum)
{
    GeoIPLookup gl;
    return GeoIP_name_by_ipnum_gl(gi, ipnum, &gl);
}
char *
GeoIP_name_by_ipnum_v6(GeoIP * gi, geoipv6_t ipnum)
{
    GeoIPLookup gl;
    return GeoIP_name_by_ipnum_v6_gl(gi, ipnum, &gl);
}
char *
GeoIP_name_by_addr(GeoIP * gi, const char *addr)
{
    GeoIPLookup gl;
    return GeoIP_name_by_addr_gl(gi, addr, &gl);
}
char *
GeoIP_name_by_addr_v6(GeoIP * gi, const char *addr)
{
    GeoIPLookup gl;
    return GeoIP_name_by_addr_v6_gl(gi, addr, &gl);
}

char *
GeoIP_name_by_name(GeoIP * gi, const char *name)
{
    GeoIPLookup gl;
    return GeoIP_name_by_name_gl(gi, name, &gl);
}

char *
GeoIP_name_by_name_v6(GeoIP * gi, const char *name)
{
    GeoIPLookup gl;
    return GeoIP_name_by_name_v6_gl(gi, name, &gl);
}
