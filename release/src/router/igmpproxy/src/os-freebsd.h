#include <net/route.h>
#include <netinet/in_systm.h>
#include <netinet/ip_mroute.h>
#include <netinet/ip.h>
#include <netinet/igmp.h>

#if __FreeBSD_version >= 800069 && defined BURN_BRIDGES \
	|| __FreeBSD_version >= 800098
#define IGMP_MEMBERSHIP_QUERY IGMP_HOST_MEMBERSHIP_QUERY
#define IGMP_V1_MEMBERSHIP_REPORT IGMP_v1_HOST_MEMBERSHIP_REPORT
#define IGMP_V2_MEMBERSHIP_REPORT IGMP_v2_HOST_MEMBERSHIP_REPORT
#define IGMP_V2_LEAVE_GROUP IGMP_HOST_LEAVE_MESSAGE
#endif
#define IGMP_V3_MEMBERSHIP_REPORT 0x22

#define INADDR_ALLIGMPV3_GROUP ((in_addr_t) 0xe0000016)

static inline u_short ip_data_len(const struct ip *ip)
{
	return ip->ip_len;
}

static inline void ip_set_len(struct ip *ip, u_short len)
{
	ip->ip_len = len;
}
