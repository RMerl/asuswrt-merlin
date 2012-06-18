#include <net/route.h>
#include <net/ip_mroute/ip_mroute.h>
#include <netinet/ip.h>
#include <netinet/igmp.h>

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
